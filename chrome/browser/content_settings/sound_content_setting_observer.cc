// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/sound_content_setting_observer.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/autoplay.mojom.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/tabs/tab_utils.h"
#endif

SoundContentSettingObserver::SoundContentSettingObserver(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      logged_site_muted_ukm_(false),
      observer_(this) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile);
  observer_.Add(host_content_settings_map_);

#if !defined(OS_ANDROID)
  // Listen to changes of the block autoplay pref.
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kBlockAutoplayEnabled,
      base::BindRepeating(&SoundContentSettingObserver::UpdateAutoplayPolicy,
                          base::Unretained(this)));
#endif
}

SoundContentSettingObserver::~SoundContentSettingObserver() = default;

void SoundContentSettingObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument())
    return;

  if (!base::FeatureList::IsEnabled(media::kAutoplayWhitelistSettings))
    return;

  GURL url = navigation_handle->IsInMainFrame()
                 ? navigation_handle->GetURL()
                 : navigation_handle->GetWebContents()->GetLastCommittedURL();

  content_settings::SettingInfo setting_info;
  std::unique_ptr<base::Value> setting =
      host_content_settings_map_->GetWebsiteSetting(
          url, navigation_handle->GetURL(),
          CONTENT_SETTINGS_TYPE_SOUND, std::string(), &setting_info);

  if (content_settings::ValueToContentSetting(setting.get()) !=
      CONTENT_SETTING_ALLOW) {
    return;
  }

  if (setting_info.source != content_settings::SETTING_SOURCE_USER)
    return;

  if (setting_info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      setting_info.secondary_pattern == ContentSettingsPattern::Wildcard()) {
    return;
  }

  blink::mojom::AutoplayConfigurationClientAssociatedPtr client;
  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&client);
  client->AddAutoplayFlags(url::Origin::Create(navigation_handle->GetURL()),
                           blink::mojom::kAutoplayFlagUserException);
}

void SoundContentSettingObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() && navigation_handle->HasCommitted() &&
      !navigation_handle->IsSameDocument()) {
    MuteOrUnmuteIfNecessary();
    logged_site_muted_ukm_ = false;
  }
}

void SoundContentSettingObserver::OnAudioStateChanged(bool audible) {
  CheckSoundBlocked(audible);
}

void SoundContentSettingObserver::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  if (content_type != CONTENT_SETTINGS_TYPE_SOUND)
    return;

#if !defined(OS_ANDROID)
  if (primary_pattern == ContentSettingsPattern() &&
      secondary_pattern == ContentSettingsPattern() &&
      resource_identifier.empty()) {
    UpdateAutoplayPolicy();
  }
#endif

  MuteOrUnmuteIfNecessary();
  CheckSoundBlocked(web_contents()->IsCurrentlyAudible());
}

void SoundContentSettingObserver::MuteOrUnmuteIfNecessary() {
  bool mute = GetCurrentContentSetting() == CONTENT_SETTING_BLOCK;

// TabMutedReason does not exist on Android.
#if defined(OS_ANDROID)
  web_contents()->SetAudioMuted(mute);
#else
  // We don't want to overwrite TabMutedReason with no change.
  if (mute == web_contents()->IsAudioMuted())
    return;

  TabMutedReason reason = chrome::GetTabAudioMutedReason(web_contents());

  // Do not unmute if we're muted due to media capture.
  if (!mute && reason == TabMutedReason::MEDIA_CAPTURE)
    return;

  // Do not override the decisions of an extension.
  if (reason == TabMutedReason::EXTENSION)
    return;

  // Don't unmute a chrome:// URL if the tab has been explicitly muted on a
  // chrome:// URL.
  if (reason == TabMutedReason::CONTENT_SETTING_CHROME &&
      web_contents()->GetLastCommittedURL().SchemeIs(
          content::kChromeUIScheme)) {
    return;
  }

  chrome::SetTabAudioMuted(web_contents(), mute,
                           TabMutedReason::CONTENT_SETTING, std::string());
#endif  // defined(OS_ANDROID)
}

ContentSetting SoundContentSettingObserver::GetCurrentContentSetting() {
  GURL url = web_contents()->GetLastCommittedURL();
  return host_content_settings_map_->GetContentSetting(
      url, url, CONTENT_SETTINGS_TYPE_SOUND, std::string());
}

void SoundContentSettingObserver::CheckSoundBlocked(bool is_audible) {
  if (is_audible && GetCurrentContentSetting() == CONTENT_SETTING_BLOCK) {
    // The tab has tried to play sound, but was muted.
    TabSpecificContentSettings* settings =
        TabSpecificContentSettings::FromWebContents(web_contents());
    if (settings)
      settings->OnAudioBlocked();

    RecordSiteMutedUKM();
  }
}

void SoundContentSettingObserver::RecordSiteMutedUKM() {
  // We only want to log 1 event per navigation.
  if (logged_site_muted_ukm_)
    return;
  logged_site_muted_ukm_ = true;

  ukm::builders::Media_SiteMuted(
      ukm::GetSourceIdForWebContentsDocument(web_contents()))
      .SetMuteReason(GetSiteMutedReason())
      .Record(ukm::UkmRecorder::Get());
}

SoundContentSettingObserver::MuteReason
SoundContentSettingObserver::GetSiteMutedReason() {
  const GURL url = web_contents()->GetLastCommittedURL();
  content_settings::SettingInfo info;
  host_content_settings_map_->GetWebsiteSetting(
      url, url, CONTENT_SETTINGS_TYPE_SOUND, std::string(), &info);

  DCHECK_EQ(content_settings::SETTING_SOURCE_USER, info.source);

  if (info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      info.secondary_pattern == ContentSettingsPattern::Wildcard()) {
    return MuteReason::kMuteByDefault;
  }
  return MuteReason::kSiteException;
}

#if !defined(OS_ANDROID)
void SoundContentSettingObserver::UpdateAutoplayPolicy() {
  // Force a WebkitPreferences update to update the autoplay policy.
  web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
}
#endif
