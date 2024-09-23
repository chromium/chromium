// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/support/origin_trial_service.h"

#include "base/metrics/histogram_functions.h"
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
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace tpcd::trial {
namespace {

const char kTrialName[] = "LimitThirdPartyCookies";

bool IsSameSite(const GURL& url1, const GURL& url2) {
  // We can't use SiteInstance::IsSameSiteWithURL() because both mainframe and
  // subframe are under default SiteInstance on low-end Android environment, and
  // it treats them as same-site even though the passed url is actually not a
  // same-site.
  return url1.SchemeIs(url2.scheme()) &&
         net::registry_controlled_domains::SameDomainOrHost(
             url1, url2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}
}  // namespace

OriginTrialService::OriginTrialService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  ot_controller_ = browser_context->GetOriginTrialsControllerDelegate();
  if (ot_controller_) {
    ot_controller_->AddObserver(this);
  }
}

OriginTrialService::~OriginTrialService() = default;

void OriginTrialService::Shutdown() {
  if (ot_controller_) {
    ot_controller_->RemoveObserver(this);
  }

  ot_controller_ = nullptr;
  browser_context_ = nullptr;
}

void OriginTrialService::UpdateTopLevelTrialSettingsForTesting(
    const url::Origin& origin,
    bool match_subdomains,
    bool enabled) {
  UpdateTopLevelTrialSettings(origin, match_subdomains, enabled);
}

void OriginTrialService::UpdateTopLevelTrialSettings(const url::Origin& origin,
                                                     bool match_subdomains,
                                                     bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const GURL origin_as_url = origin.GetURL();

  ContentSettingsPattern primary_setting_pattern;
  if (match_subdomains) {
    // `ContentSettingsPattern::FromURL()` returns a pattern with a domain
    // wildcard by default.
    primary_setting_pattern = ContentSettingsPattern::FromURL(origin_as_url);
  } else {
    primary_setting_pattern =
        ContentSettingsPattern::FromURLNoWildcard(origin_as_url);
  }

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context_);
  CHECK(settings_map);

  // Check for an existing `TOP_LEVEL_TPCD_ORIGIN_TRIAL` setting that allows
  // `origin`. Only the primary pattern is checked since the secondary pattern
  // is unused (i.e., wildcard) for this content settings type's scope
  // (`TOP_ORIGIN_ONLY_SCOPE`).
  content_settings::SettingInfo existing_setting_info;
  bool setting_exists =
      (settings_map->GetContentSetting(
           origin_as_url, GURL(),
           ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL,
           &existing_setting_info) == CONTENT_SETTING_BLOCK) &&
      (existing_setting_info.primary_pattern == primary_setting_pattern);

  // If the trial status matches existing settings, there is no need to
  // update `settings_map`.
  if (enabled == setting_exists) {
    return;
  }

  if (enabled) {
    settings_map->SetContentSettingCustomScope(
        primary_setting_pattern, ContentSettingsPattern::Wildcard(),
        ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL,
        CONTENT_SETTING_BLOCK);
  } else {
    CHECK(setting_exists);

    // The trial has been disabled for origin, so remove the associated setting.
    auto matches_pattern =
        [&](const ContentSettingPatternSource& setting) -> bool {
      // Only check the primary_pattern since the settings type uses
      // `TOP_ORIGIN_ONLY_SCOPE`.
      return (setting.primary_pattern == existing_setting_info.primary_pattern);
    };

    settings_map->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL, matches_pattern);
  }

  ContentSettingsForOneType trial_settings =
      settings_map->GetSettingsForOneType(
          ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL);

  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettings(ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL,
                           std::move(trial_settings), base::NullCallback());
}

void OriginTrialService::ClearTopLevelTrialSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context_);
  CHECK(settings_map);

  settings_map->ClearSettingsForOneType(
      ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL);
}

void OriginTrialService::OnStatusChanged(
    const content::OriginTrialStatusChangeDetails& details) {
  // LimitThirdPartyCookies is a first-party trial that is only intended for use
  // by top-level sites. However, a cross-site iframe may enable a first-party
  // origin trial (for itself), so to ensure we only create settings for
  // top-level sites, explicitly check that `origin` is same-site with
  // `partition_site`.
  if (!IsSameSite(details.origin.GetURL(), GURL(details.partition_site))) {
    return;
  }

  UpdateTopLevelTrialSettings(details.origin, details.match_subdomains,
                              details.enabled);
}

void OriginTrialService::OnPersistedTokensCleared() {
  ClearTopLevelTrialSettings();
}

std::string OriginTrialService::trial_name() {
  return kTrialName;
}

}  // namespace tpcd::trial
