// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/types/optional_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"

namespace first_party_sets {

namespace {

using ServiceState = FirstPartySetsPolicyService::ServiceState;

network::mojom::FirstPartySetsReadyEventPtr MakeReadyEvent(
    net::FirstPartySetsContextConfig config,
    net::FirstPartySetsCacheFilter cache_filter) {
  auto ready_event = network::mojom::FirstPartySetsReadyEvent::New();
  ready_event->config = std::move(config);
  ready_event->cache_filter = std::move(cache_filter);
  return ready_event;
}

const base::Value::Dict* GetOverridesPolicyForProfile(
    const PrefService* prefs) {
  if (!prefs) {
    return nullptr;
  }
  // The value is declared as a dict, but we assume that the user may have
  // modified the prefs file or the file may be corrupt.
  return prefs->GetValue(first_party_sets::kRelatedWebsiteSetsOverrides)
      .GetIfDict();
}

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

  if (service_state_ == ServiceState::kPermanentlyDisabled) {
    OnReadyToNotifyDelegates(net::FirstPartySetsContextConfig(),
                             net::FirstPartySetsCacheFilter());
    return;
  }

  // Immediately retrieve the associated FirstPartySetsContextConfig. We can do
  // this since the value of the FirstPartySets Overrides policy doesn't
  // dynamically refresh, and all the delegates for `context` will have the same
  // policy and thus the same config.
  content::FirstPartySetsHandler::GetInstance()->GetContextConfigForPolicy(
      GetOverridesPolicyForProfile(profile->GetPrefs()),
      base::BindOnce(&FirstPartySetsPolicyService::OnProfileConfigReady,
                     weak_factory_.GetWeakPtr(),
                     // We should only clear site data if First-Party Sets is
                     // enabled when the service is created, to allow users
                     // to play with the FPS enabled setting without
                     // affecting user experience during the browser session.
                     service_state_));
}

void FirstPartySetsPolicyService::ComputeFirstPartySetMetadata(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_enabled()) {
    std::move(callback).Run({});
    return;
  }

  if (!config_.has_value()) {
    on_ready_callbacks_.push_back(base::BindOnce(
        &FirstPartySetsPolicyService::ComputeFirstPartySetMetadataInternal,
        weak_factory_.GetWeakPtr(), site, base::OptionalFromPtr(top_frame_site),
        std::move(callback)));
    return;
  }

  content::FirstPartySetsHandler::GetInstance()->ComputeFirstPartySetMetadata(
      site, top_frame_site, *config_, std::move(callback));
}

void FirstPartySetsPolicyService::ComputeFirstPartySetMetadataInternal(
    const net::SchemefulSite& site,
    const std::optional<net::SchemefulSite>& top_frame_site,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config_.has_value());

  if (!is_enabled()) {
    std::move(callback).Run({});
    return;
  }

  content::FirstPartySetsHandler::GetInstance()->ComputeFirstPartySetMetadata(
      site, base::OptionalToPtr(top_frame_site), *config_, std::move(callback));
}

void FirstPartySetsPolicyService::AddRemoteAccessDelegate(
    mojo::Remote<network::mojom::FirstPartySetsAccessDelegate>
        access_delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_delegate->SetEnabled(is_enabled());
  if (config_.has_value() && cache_filter_.has_value()) {
    // Since the list of First-Party Sets is static after initialization and
    // the FirstPartySetsOverrides policy doesn't support dynamic refresh, a
    // profile's `config_` is static as well.
    access_delegate->NotifyReady(
        MakeReadyEvent(config_->Clone(), cache_filter_->Clone()));
  }
  access_delegates_.Add(std::move(access_delegate));
}

void FirstPartySetsPolicyService::OnFirstPartySetsEnabledChanged(bool enabled) {
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
  for (auto& delegate : access_delegates_) {
    delegate->SetEnabled(is_enabled());
  }

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
        return content_settings::IsGrantedByRelatedWebsiteSets(
            ContentSettingsType::STORAGE_ACCESS, setting.metadata);
      });
  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      [](const ContentSettingPatternSource& setting) -> bool {
        return content_settings::IsGrantedByRelatedWebsiteSets(
            ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, setting.metadata);
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
  access_delegates_.Clear();
  on_ready_callbacks_.clear();
  privacy_sandbox_settings_observer_.Reset();
  weak_factory_.InvalidateWeakPtrs();
}

void FirstPartySetsPolicyService::WaitForFirstInitCompleteForTesting(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!on_first_init_complete_for_testing_.has_value());
  if (first_initialization_complete_for_testing_) {
    CHECK(config_.has_value());
    std::move(callback).Run();
    return;
  }
  on_first_init_complete_for_testing_ = std::move(callback);
}

void FirstPartySetsPolicyService::OnProfileConfigReady(
    ServiceState initial_state,
    net::FirstPartySetsContextConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(initial_state, ServiceState::kPermanentlyDisabled);

  if (initial_state == ServiceState::kDisabled) {
    OnReadyToNotifyDelegates(std::move(config),
                             net::FirstPartySetsCacheFilter());
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  CHECK(profile);
  if (!profile->IsRegularProfile() || profile->IsGuestSession()) {
    // TODO(crbug.com/40233408): regular profiles and guest sessions
    // aren't mutually exclusive on ChromeOS.
    OnReadyToNotifyDelegates(std::move(config),
                             net::FirstPartySetsCacheFilter());
    return;
  }

  // Representation of the current profile to be persisted on disk.
  const std::string browser_context_id = profile->GetBaseName().AsUTF8Unsafe();

  base::RepeatingCallback<content::BrowserContext*()> browser_context_getter =
      base::BindRepeating(
          [](base::WeakPtr<FirstPartySetsPolicyService> weak_ptr) {
            return weak_ptr ? weak_ptr->browser_context() : nullptr;
          },
          weak_factory_.GetWeakPtr());

  content::FirstPartySetsHandler::GetInstance()
      ->ClearSiteDataOnChangedSetsForContext(
          browser_context_getter, browser_context_id, std::move(config),
          base::BindOnce(&FirstPartySetsPolicyService::OnReadyToNotifyDelegates,
                         weak_factory_.GetWeakPtr()));
}

std::optional<net::FirstPartySetEntry> FirstPartySetsPolicyService::FindEntry(
    const net::SchemefulSite& site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!config_.has_value() || !is_enabled()) {
    return std::nullopt;
  }

  return content::FirstPartySetsHandler::GetInstance()->FindEntry(
      site, config_.value());
}

bool FirstPartySetsPolicyService::IsSiteInManagedSet(
    const net::SchemefulSite& site) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!config_.has_value() || !is_enabled()) {
    return false;
  }

  std::optional<net::FirstPartySetEntryOverride> maybe_override =
      config_->FindOverride(site);
  return maybe_override.has_value() && !maybe_override->IsDeletion();
}

bool FirstPartySetsPolicyService::ForEachEffectiveSetEntry(
    base::FunctionRef<bool(const net::SchemefulSite&,
                           const net::FirstPartySetEntry&)> f) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_enabled() || !is_ready()) {
    return false;
  }
  return content::FirstPartySetsHandler::GetInstance()
      ->ForEachEffectiveSetEntry(config_.value(), f);
}

void FirstPartySetsPolicyService::OnReadyToNotifyDelegates(
    net::FirstPartySetsContextConfig config,
    net::FirstPartySetsCacheFilter cache_filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_ = std::move(config);
  cache_filter_ = std::move(cache_filter);
  first_initialization_complete_for_testing_ = true;
  for (auto& delegate : access_delegates_) {
    delegate->NotifyReady(
        MakeReadyEvent(config_.value().Clone(), cache_filter_.value().Clone()));
  }

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
  access_delegates_.Clear();
  on_ready_callbacks_.clear();
  config_.reset();
  cache_filter_.reset();
  on_first_init_complete_for_testing_.reset();
  // Note: `first_initialization_complete_for_testing_` is intentionally not
  // reset here.
}

}  // namespace first_party_sets
