// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/support/top_level_trial_service.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/features.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace tpcd::trial {
namespace {

const char kTrialName[] = "TopLevelTpcd";
}  // namespace

TopLevelTrialService::TopLevelTrialService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  ot_controller_ = browser_context->GetOriginTrialsControllerDelegate();
  if (ot_controller_) {
    ot_controller_->AddObserver(this);
  }
}

TopLevelTrialService::~TopLevelTrialService() = default;

void TopLevelTrialService::Shutdown() {
  if (ot_controller_) {
    ot_controller_->RemoveObserver(this);
  }

  ot_controller_ = nullptr;
  browser_context_ = nullptr;
}

void TopLevelTrialService::UpdateTopLevelTrialSettingsForTesting(
    const url::Origin& origin,
    bool match_subdomains,
    bool enabled) {
  UpdateTopLevelTrialSettings(origin, match_subdomains, enabled);
}

void TopLevelTrialService::UpdateTopLevelTrialSettings(
    const url::Origin& origin,
    bool includes_subdomains,
    bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context_);
  CHECK(settings_map);

  const GURL origin_as_url = origin.GetURL();

  // Check for an existing `TOP_LEVEL_TPCD_TRIAL` setting that allows origin.
  content_settings::SettingInfo existing_setting_info;
  bool setting_exists =
      (settings_map->GetContentSetting(
           origin_as_url, origin_as_url,
           ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
           &existing_setting_info) == CONTENT_SETTING_ALLOW) &&
      (existing_setting_info.primary_pattern.HasDomainWildcard() ==
       includes_subdomains) &&
      !existing_setting_info.primary_pattern.MatchesAllHosts();

  // If the trial status matches existing settings, there is no need to
  // update `settings_map`.
  if (enabled == setting_exists) {
    return;
  }

  ContentSettingsPattern primary_setting_pattern;
  ContentSettingsPattern secondary_setting_pattern =
      ContentSettingsPattern::Wildcard();
  if (includes_subdomains) {
    primary_setting_pattern = ContentSettingsPattern::FromURL(origin_as_url);
  } else {
    // In this case, the combination of `primary_setting_pattern` and
    // `secondary_setting_pattern` is equivalent to
    // `ContentSettingsType::TOP_LEVEL_TPCD_TRIAL`'s default scope
    // (`TOP_ORIGIN_ONLY_SCOPE`).
    primary_setting_pattern =
        ContentSettingsPattern::FromURLNoWildcard(origin_as_url);
  }

  if (enabled) {
    settings_map->SetContentSettingCustomScope(
        primary_setting_pattern, secondary_setting_pattern,
        ContentSettingsType::TOP_LEVEL_TPCD_TRIAL, CONTENT_SETTING_ALLOW);
  } else {
    CHECK(setting_exists);

    // Remove settings for expired/unused pairs to avoid memory bloat.
    auto matches_pair =
        [&](const ContentSettingPatternSource& setting) -> bool {
      return (setting.primary_pattern ==
              existing_setting_info.primary_pattern) &&
             (setting.secondary_pattern ==
              existing_setting_info.secondary_pattern);
    };

    settings_map->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::TOP_LEVEL_TPCD_TRIAL, matches_pair);
  }

  ContentSettingsForOneType trial_settings =
      settings_map->GetSettingsForOneType(
          ContentSettingsType::TOP_LEVEL_TPCD_TRIAL);

  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettings(ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
                           std::move(trial_settings), base::NullCallback());
}

void TopLevelTrialService::ClearTopLevelTrialSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context_);
  CHECK(settings_map);

  settings_map->ClearSettingsForOneType(
      ContentSettingsType::TOP_LEVEL_TPCD_TRIAL);
}

void TopLevelTrialService::OnStatusChanged(const url::Origin& origin,
                                           const std::string& partition_site,
                                           bool includes_subdomains,
                                           bool enabled) {
  // TopLevelTpcd is a first-party trial, so the |partition_site| can be ignored
  // (and should always be same-site with the |origin| anyway).
  UpdateTopLevelTrialSettings(origin, includes_subdomains, enabled);
}

void TopLevelTrialService::OnPersistedTokensCleared() {
  ClearTopLevelTrialSettings();
}

std::string TopLevelTrialService::trial_name() {
  return kTrialName;
}

}  // namespace tpcd::trial
