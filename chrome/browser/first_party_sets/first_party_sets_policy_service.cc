// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"

namespace first_party_sets {

namespace {

using ServiceState = FirstPartySetsPolicyService::ServiceState;

ServiceState GetServiceState(Profile* profile, bool pref_enabled) {
  if (profile->IsSystemProfile() || profile->IsGuestSession() ||
      profile->IsOffTheRecord()) {
    return ServiceState::kPermanentlyDisabled;
  }
  if (base::FeatureList::IsEnabled(
          net::features::kForceThirdPartyCookieBlocking)) {
    return ServiceState::kPermanentlyEnabled;
  }
  return pref_enabled ? ServiceState::kEnabled : ServiceState::kDisabled;
}

}  // namespace

FirstPartySetsPolicyService::FirstPartySetsPolicyService(
    content::BrowserContext* browser_context)
    : browser_context_(
          raw_ref<content::BrowserContext>::from_ptr(browser_context)),
      privacy_sandbox_settings_(
          raw_ref<privacy_sandbox::PrivacySandboxSettings>::from_ptr(
              PrivacySandboxSettingsFactory::GetForProfile(
                  Profile::FromBrowserContext(browser_context)))) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  privacy_sandbox_settings_observer_.Observe(&*privacy_sandbox_settings_);
  Init();
}

FirstPartySetsPolicyService::~FirstPartySetsPolicyService() = default;

void FirstPartySetsPolicyService::InitForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Init();
}

void FirstPartySetsPolicyService::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  // profile is guaranteed to be non-null since we create this service with a
  // non-null `context`.
  CHECK(profile);

  service_state_ = GetServiceState(
      profile, privacy_sandbox_settings_->AreRelatedWebsiteSetsEnabled());

  if (service_state_ == ServiceState::kPermanentlyDisabled ||
      service_state_ == ServiceState::kDisabled) {
    OnReadyToNotifyDelegates();
    return;
  }

  if (!profile->IsRegularProfile() || profile->IsGuestSession()) {
    // TODO(crbug.com/40233408): regular profiles and guest sessions
    // aren't mutually exclusive on ChromeOS.
    OnReadyToNotifyDelegates();
    return;
  }

  content::FirstPartySetsHandler* handler =
      content::FirstPartySetsHandler::GetInstance();
  if (handler->WhenInitComplete(
          base::BindOnce(&FirstPartySetsPolicyService::OnReadyToNotifyDelegates,
                         weak_factory_.GetWeakPtr()))) {
    OnReadyToNotifyDelegates();
  }
}

void FirstPartySetsPolicyService::ComputeFirstPartySetMetadata(
    const net::SchemefulSite& site,
    base::optional_ref<const net::SchemefulSite> top_frame_site,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_enabled()) {
    std::move(callback).Run({});
    return;
  }

  if (!is_ready()) {
    on_ready_callbacks_.push_back(base::BindOnce(
        &FirstPartySetsPolicyService::ComputeFirstPartySetMetadataInternal,
        weak_factory_.GetWeakPtr(), site, top_frame_site.CopyAsOptional(),
        std::move(callback)));
    return;
  }

  ComputeFirstPartySetMetadataInternal(site, top_frame_site,
                                       std::move(callback));
}

void FirstPartySetsPolicyService::ComputeFirstPartySetMetadataInternal(
    const net::SchemefulSite& site,
    base::optional_ref<const net::SchemefulSite> top_frame_site,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_ready());

  if (!is_enabled()) {
    std::move(callback).Run({});
    return;
  }

  content::FirstPartySetsHandler::GetInstance()->ComputeFirstPartySetMetadata(
      site, top_frame_site, net::FirstPartySetsContextConfig(),
      std::move(callback));
}

void FirstPartySetsPolicyService::OnRelatedWebsiteSetsEnabledChanged(
    bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (service_state_ == ServiceState::kPermanentlyDisabled ||
      service_state_ == ServiceState::kPermanentlyEnabled) {
    return;
  }
  // TODO(crbug.com/1366846) Add metrics here to track whether the pref is ever
  // enabled before the config is ready to be to be sent to the delegates.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  CHECK(profile);
  service_state_ = GetServiceState(profile, enabled);

  // Clear all the existing permission decisions that were made by FPS, since
  // the enabled/disabled state of FPS has now changed.
  ClearContentSettings(profile);
  for (Profile* otr_profile : profile->GetAllOffTheRecordProfiles()) {
    ClearContentSettings(otr_profile);
  }
}

void FirstPartySetsPolicyService::ClearContentSettings(Profile* profile) const {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::STORAGE_ACCESS,
      [](const ContentSettingPatternSource& setting) -> bool {
        return setting.metadata.decided_by_related_website_sets();
      });
  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      [](const ContentSettingPatternSource& setting) -> bool {
        return setting.metadata.decided_by_related_website_sets();
      });
}

void FirstPartySetsPolicyService::RegisterThrottleResumeCallback(
    base::OnceClosure resume_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_ready());
  CHECK(is_enabled());
  on_ready_callbacks_.push_back(std::move(resume_callback));
}

void FirstPartySetsPolicyService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_ready_callbacks_.clear();
  privacy_sandbox_settings_observer_.Reset();
  weak_factory_.InvalidateWeakPtrs();
}

void FirstPartySetsPolicyService::WaitForFirstInitCompleteForTesting(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!on_first_init_complete_for_testing_.has_value());
  if (first_initialization_complete_for_testing_) {
    CHECK(is_ready_);
    std::move(callback).Run();
    return;
  }
  on_first_init_complete_for_testing_ = std::move(callback);
}

std::optional<net::FirstPartySetEntry> FirstPartySetsPolicyService::FindEntry(
    const net::SchemefulSite& site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_ready() || !is_enabled()) {
    return std::nullopt;
  }

  return content::FirstPartySetsHandler::GetInstance()->FindEntry(
      site, net::FirstPartySetsContextConfig());
}

bool FirstPartySetsPolicyService::IsSiteInManagedSet(
    const net::SchemefulSite& site) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

bool FirstPartySetsPolicyService::ForEachEffectiveSetEntry(
    base::FunctionRef<bool(const net::SchemefulSite&,
                           const net::FirstPartySetEntry&)> f) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_enabled() || !is_ready()) {
    return false;
  }
  return content::FirstPartySetsHandler::GetInstance()
      ->ForEachEffectiveSetEntry(net::FirstPartySetsContextConfig(), f);
}

void FirstPartySetsPolicyService::OnReadyToNotifyDelegates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_ready_ = true;
  first_initialization_complete_for_testing_ = true;

  base::circular_deque<base::OnceClosure> callback_queue;
  callback_queue.swap(on_ready_callbacks_);
  while (!callback_queue.empty()) {
    base::OnceClosure callback = std::move(callback_queue.front());
    callback_queue.pop_front();
    std::move(callback).Run();
  }

  if (on_first_init_complete_for_testing_.has_value()) {
    std::move(on_first_init_complete_for_testing_).value().Run();
  }
}

void FirstPartySetsPolicyService::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_state_ = ServiceState::kEnabled;
  on_ready_callbacks_.clear();
  is_ready_ = false;
  on_first_init_complete_for_testing_.reset();
  // Note: `first_initialization_complete_for_testing_` is intentionally not
  // reset here.
}

}  // namespace first_party_sets
