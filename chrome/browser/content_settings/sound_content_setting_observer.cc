// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/sound_content_setting_observer.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/url_constants.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#endif

using content_settings::SettingSource;

SoundContentSettingObserver::SoundContentSettingObserver(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<SoundContentSettingObserver>(*contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile);
  observation_.Observe(host_content_settings_map_.get());

#if !BUILDFLAG(IS_ANDROID)
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

  GURL url = !navigation_handle->GetParentFrameOrOuterDocument()
                 ? navigation_handle->GetURL()
                 : navigation_handle->GetRenderFrameHost()
                       ->GetOutermostMainFrame()
                       ->GetLastCommittedURL();

  content_settings::SettingInfo setting_info;
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      url, url, ContentSettingsType::SOUND, &setting_info);

  if (setting != CONTENT_SETTING_ALLOW) {
    return;
  }

  if (setting_info.source != SettingSource::kUser) {
    return;
  }

  if (setting_info.primary_pattern.MatchesAllHosts() &&
      setting_info.secondary_pattern.MatchesAllHosts()) {
    return;
  }

  mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&client);
  client->AddAutoplayFlags(url::Origin::Create(navigation_handle->GetURL()),
                           blink::mojom::kAutoplayFlagUserException);
}

void SoundContentSettingObserver::PrimaryPageChanged(content::Page& page) {
  MuteOrUnmuteIfNecessary();
  logged_site_muted_ukm_ = false;
}

void SoundContentSettingObserver::OnAudioStateChanged(bool audible) {
  CheckSoundBlocked(audible);
}

void SoundContentSettingObserver::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (!content_type_set.Contains(ContentSettingsType::SOUND))
    return;

#if !BUILDFLAG(IS_ANDROID)
  if (primary_pattern.MatchesAllHosts() &&
      secondary_pattern.MatchesAllHosts()) {
    UpdateAutoplayPolicy();
  }
#endif

  MuteOrUnmuteIfNecessary();
  CheckSoundBlocked(web_contents()->IsCurrentlyAudible());
}

void SoundContentSettingObserver::MuteOrUnmuteIfNecessary() {
  bool mute = GetCurrentContentSetting() == CONTENT_SETTING_BLOCK;

// TabMutedReason does not exist on Android.
#if BUILDFLAG(IS_ANDROID)
  web_contents()->SetAudioMuted(mute);
#else
  // We don't want to overwrite TabMutedReason with no change.
  if (mute == web_contents()->IsAudioMuted())
    return;

  TabMutedReason reason = GetTabAudioMutedReason(web_contents());

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

  // Do not unmute if we're muted due to audio indicator.
  if (!mute && reason == TabMutedReason::AUDIO_INDICATOR)
    return;

  SetTabAudioMuted(web_contents(), mute, TabMutedReason::CONTENT_SETTING,
                   std::string());
#endif  // BUILDFLAG(IS_ANDROID)
}

ContentSetting SoundContentSettingObserver::GetCurrentContentSetting() {
  GURL url = web_contents()->GetLastCommittedURL();
  return host_content_settings_map_->GetContentSetting(
      url, url, ContentSettingsType::SOUND);
}

void SoundContentSettingObserver::CheckSoundBlocked(bool is_audible) {
  if (is_audible && GetCurrentContentSetting() == CONTENT_SETTING_BLOCK) {
    // Since this is a page-level event and only primary pages can play audio
    // in prerendering, we get `settings` from the main frame of the primary
    // page.
    // TODO(crbug.com/40139135): For other types of FrameTrees (fenced
    // frames) than prerendering, we should figure a way of not having
    // to use GetPrimaryMainFrame here. (pass the source frame somehow)
    content_settings::PageSpecificContentSettings* settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            web_contents()->GetPrimaryMainFrame());
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
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId())
      .SetMuteReason(GetSiteMutedReason())
      .Record(ukm::UkmRecorder::Get());
}

SoundContentSettingObserver::MuteReason
SoundContentSettingObserver::GetSiteMutedReason() {
  const GURL url = web_contents()->GetLastCommittedURL();
  content_settings::SettingInfo info;
  host_content_settings_map_->GetWebsiteSetting(
      url, url, ContentSettingsType::SOUND, &info);

  DCHECK_EQ(SettingSource::kUser, info.source);

  if (info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      info.secondary_pattern == ContentSettingsPattern::Wildcard()) {
    return MuteReason::kMuteByDefault;
  }
  return MuteReason::kSiteException;
}

#if !BUILDFLAG(IS_ANDROID)
void SoundContentSettingObserver::UpdateAutoplayPolicy() {
  // Force a WebkitPreferences update to update the autoplay policy.
  web_contents()->OnWebPreferencesChanged();
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(SoundContentSettingObserver);
