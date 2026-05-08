// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_service.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/component_updater/indigo_component_installer.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/indigo/api_client.h"
#include "chrome/browser/indigo/indigo_extension_utils.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace indigo {

CombinedEligibility::CombinedEligibility() = default;
CombinedEligibility::CombinedEligibility(const CombinedEligibility&) = default;
CombinedEligibility& CombinedEligibility::operator=(
    const CombinedEligibility&) = default;
CombinedEligibility::CombinedEligibility(CombinedEligibility&&) = default;
CombinedEligibility& CombinedEligibility::operator=(CombinedEligibility&&) =
    default;
CombinedEligibility::~CombinedEligibility() = default;

bool CombinedEligibility::CanGenerateImage() const {
  return local_eligibility == LocalEligibility::kEligible &&
         remote_eligibility.has_value() &&
         remote_eligibility->is_service_supported_for_account &&
         remote_eligibility->has_user_image && has_onboarded_pref;
}

bool CombinedEligibility::ReadyToOnboard() const {
  if (local_eligibility != LocalEligibility::kEligible ||
      !remote_eligibility.has_value() ||
      !remote_eligibility->is_service_supported_for_account) {
    return false;
  }
  return !remote_eligibility->has_user_image || !has_onboarded_pref;
}

// static
std::optional<base::FilePath> IndigoService::GetScriptPath() {
  static constexpr char kIndigoScriptSwitch[] = "indigo-script";
  base::FilePath override_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kIndigoScriptSwitch);
  if (!override_path.empty()) {
    return override_path;
  }
  return component_updater::GetIndigoContentScriptPath();
}

IndigoService::IndigoService(Profile* profile,
                             signin::IdentityManager* identity_manager,
                             PrefService* pref_service)
    : profile_(profile),
      identity_manager_(identity_manager),
      pref_service_(pref_service) {
  CHECK(base::FeatureList::IsEnabled(features::kIndigo));
  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }

  if (pref_service_) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(pref_service_);
    pref_change_registrar_->Add(
        prefs::kIndigoPolicy,
        base::BindRepeating(&IndigoService::UpdateLocalEligibilityAndNotify,
                            base::Unretained(this)));
  }

  last_known_local_eligibility_ = ComputeLocalEligibility();
  api_client_ = std::make_unique<ApiClient>(
      identity_manager, profile->GetDefaultStoragePartition()
                            ->GetURLLoaderFactoryForBrowserProcess());

  // Register component extension for Indigo.
  extensions::ComponentLoader::Get(profile_)->Add(
      indigo_extension_utils::GetManifest(),
      base::FilePath(FILE_PATH_LITERAL("indigo")));

  indigo_component_ready_subscription_ =
      component_updater::RegisterIndigoComponentReadyCallback(
          base::BindRepeating(&IndigoService::OnIndigoComponentReady,
                              base::Unretained(this)));
}

IndigoService::~IndigoService() = default;

void IndigoService::Shutdown() {
  identity_manager_observation_.Reset();
  pref_change_registrar_.reset();
  remote_eligibility_weak_factory_.InvalidateWeakPtrs();
}

void IndigoService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kNone) {
    UpdateLocalEligibilityAndNotify();
  }
}

void IndigoService::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  UpdateLocalEligibilityAndNotify();
}

base::CallbackListSubscription
IndigoService::RegisterLocalEligibilityChangedCallback(
    LocalEligibilityChangedCallback callback) {
  return local_eligibility_callback_list_.Add(std::move(callback));
}

bool IndigoService::CanShowAnchoredMessage() const {
  return base::TimeTicks::Now() >= anchored_message_not_before_;
}

void IndigoService::AnchoredMessageShown() {
  anchored_message_not_before_ =
      base::TimeTicks::Now() +
      features::kIndigoAnchoredMessageResetDuration.Get();
}

LocalEligibility IndigoService::ComputeLocalEligibility() const {
  if (!GetScriptPath().has_value()) {
    return LocalEligibility::kMissingScript;
  }

  if (pref_service_) {
    int policy_val = pref_service_->GetInteger(prefs::kIndigoPolicy);
    if (policy_val != prefs::Policy::kAllowed) {
      return LocalEligibility::kDisabledByPolicy;
    }
  }

  CoreAccountId account_id = identity_manager_
                                 ? identity_manager_->GetPrimaryAccountId(
                                       signin::ConsentLevel::kSignin)
                                 : CoreAccountId();
  if (account_id.empty()) {
    return LocalEligibility::kNotSignedIn;
  }

  AccountInfo info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  if (info.capabilities.can_use_model_execution_features() !=
      signin::Tribool::kTrue) {
    return LocalEligibility::kMissingCapabilities;
  }

  return LocalEligibility::kEligible;
}

void IndigoService::UpdateLocalEligibilityAndNotify() {
  LocalEligibility new_eligibility = ComputeLocalEligibility();
  if (new_eligibility != last_known_local_eligibility_) {
    last_known_local_eligibility_ = new_eligibility;
    local_eligibility_callback_list_.Notify(new_eligibility);
  }
}

void IndigoService::OnIndigoComponentReady() {
  UpdateLocalEligibilityAndNotify();
}

void IndigoService::GetCombinedEligibility(
    CombinedEligibilityCallback callback) {
  CombinedEligibility status;
  status.local_eligibility = GetLocalEligibility();

  if (pref_service_) {
    status.has_onboarded_pref =
        pref_service_->GetBoolean(prefs::kIndigoHasOnboarded);
  }

  if (status.local_eligibility != LocalEligibility::kEligible) {
    std::move(callback).Run(status);
    return;
  }

  if (remote_eligibility_.has_value()) {
    status.remote_eligibility = remote_eligibility_.value();
    std::move(callback).Run(status);
    return;
  }

  pending_callbacks_.push_back(std::move(callback));
  if (remote_eligibility_fetch_in_progress_) {
    return;
  }

  TriggerRemoteEligibilityFetch();
}

void IndigoService::TriggerRemoteEligibilityFetch() {
  remote_eligibility_fetch_in_progress_ = true;
  RemoteEligibilityCallback on_rpc_status_received =
      base::BindOnce(&IndigoService::OnRemoteEligibilityReceived,
                     remote_eligibility_weak_factory_.GetWeakPtr());

  if (remote_eligibility_fetcher_) {
    remote_eligibility_fetcher_.Run(std::move(on_rpc_status_received));
    return;
  }

  api_client_->GetStatus(base::BindOnce(
      [](RemoteEligibilityCallback callback,
         base::expected<StatusResult, StatusError> result) {
        if (!result.has_value()) {
          std::move(callback).Run(base::unexpected(result.error().message));
          return;
        }
        std::move(callback).Run(
            RemoteEligibility{.is_service_supported_for_account = true,
                              .has_user_image = result.value().has_user_image});
      },
      std::move(on_rpc_status_received)));
}

void IndigoService::InvalidateRemoteEligibility() {
  remote_eligibility_.reset();
  remote_eligibility_fetch_in_progress_ = false;
  remote_eligibility_weak_factory_.InvalidateWeakPtrs();

  if (!pending_callbacks_.empty()) {
    TriggerRemoteEligibilityFetch();
  }
}

void IndigoService::OnRemoteEligibilityReceived(
    base::expected<RemoteEligibility, std::string> eligibility_or_error) {
  remote_eligibility_fetch_in_progress_ = false;
  remote_eligibility_ = std::move(eligibility_or_error);

  std::vector<CombinedEligibilityCallback> callbacks;
  callbacks.swap(pending_callbacks_);

  CombinedEligibility status;
  status.local_eligibility = GetLocalEligibility();
  if (pref_service_) {
    status.has_onboarded_pref =
        pref_service_->GetBoolean(prefs::kIndigoHasOnboarded);
  }
  status.remote_eligibility = remote_eligibility_.value();

  for (auto& callback : callbacks) {
    std::move(callback).Run(status);
  }
}

void IndigoService::SetRemoteEligibilityFetcherForTesting(
    RemoteEligibilityFetcher fetcher) {
  remote_eligibility_fetcher_ = std::move(fetcher);
}

}  // namespace indigo
