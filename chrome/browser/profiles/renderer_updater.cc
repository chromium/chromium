// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/renderer_updater.h"

#include <utility>

#include "base/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/content_settings_manager_delegate.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/common/content_settings_manager.mojom.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/signin/merge_session_throttling_utils.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager_factory.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)

// By default, JavaScript, images and auto dark are allowed, and blockable mixed
// content is blocked in guest content
void GetGuestViewDefaultContentSettingRules(
    bool incognito,
    RendererContentSettingRules* rules) {
  rules->image_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
      std::string(), incognito));
  rules->auto_dark_content_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
      std::string(), incognito));
  rules->script_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
      std::string(), incognito));
  rules->mixed_content_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK),
      std::string(), incognito));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

bool IsRendererSupportedContentSettingsTypeSet(
    ContentSettingsTypeSet content_type_set) {
  // ContainsAllTypes() signals that multiple content settings may have been
  // updated, e.g. by the PolicyProvider. This should always be sent to the
  // renderer in case a relevant setting is updated.
  if (content_type_set.ContainsAllTypes())
    return true;

  return RendererContentSettingRules::IsRendererContentSetting(
      content_type_set.GetType());
}

bool ShouldSendUpdatedContentSettingsRulesToRenderer(
    content::RenderProcessHost* render_process_host) {
  // Some renderers use manually-crafted rules that aren't meant to be updated.
  return !render_process_host->IsForGuestsOnly() &&
         !render_process_host->IsPdf();
}

}  // namespace

RendererUpdater::RendererUpdater(Profile* profile)
    : profile_(profile),
      is_off_the_record_(profile_->IsOffTheRecord()),
      original_profile_(profile->GetOriginalProfile()) {
  identity_manager_observation_.Observe(
      IdentityManagerFactory::GetForProfile(original_profile_));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  oauth2_login_manager_ =
      ash::OAuth2LoginManagerFactory::GetForProfile(original_profile_);
  oauth2_login_manager_->AddObserver(this);
  merge_session_running_ =
      ash::merge_session_throttling_utils::ShouldDelayRequestForProfile(
          original_profile_);
#endif

  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  host_content_settings_map_observation_.Observe(
      host_content_settings_map_.get());

  PrefService* pref_service = profile_->GetPrefs();
  force_google_safesearch_.Init(prefs::kForceGoogleSafeSearch, pref_service);
  force_youtube_restrict_.Init(prefs::kForceYouTubeRestrict, pref_service);
  allowed_domains_for_apps_.Init(prefs::kAllowedDomainsForApps, pref_service);

  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kForceGoogleSafeSearch,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this), kUpdateDynamicParams));
  pref_change_registrar_.Add(
      prefs::kForceYouTubeRestrict,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this), kUpdateDynamicParams));
  pref_change_registrar_.Add(
      prefs::kAllowedDomainsForApps,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this), kUpdateDynamicParams));
}

RendererUpdater::~RendererUpdater() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(!oauth2_login_manager_);
#endif
  DCHECK(!host_content_settings_map_);
}

void RendererUpdater::Shutdown() {
  pref_change_registrar_.RemoveAll();
  host_content_settings_map_observation_.Reset();
  host_content_settings_map_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  oauth2_login_manager_->RemoveObserver(this);
  oauth2_login_manager_ = nullptr;
#endif
  identity_manager_observation_.Reset();
}

void RendererUpdater::InitializeRenderer(
    content::RenderProcessHost* render_process_host) {
  DCHECK_EQ(profile_, Profile::FromBrowserContext(
                          render_process_host->GetBrowserContext()));
  auto renderer_configuration = GetRendererConfiguration(render_process_host);

  mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
      chromeos_listener_receiver;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (merge_session_running_) {
    mojo::Remote<chrome::mojom::ChromeOSListener> chromeos_listener;
    chromeos_listener_receiver = chromeos_listener.BindNewPipeAndPassReceiver();
    chromeos_listeners_.push_back(std::move(chromeos_listener));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  mojo::PendingRemote<content_settings::mojom::ContentSettingsManager>
      content_settings_manager;
  if (base::FeatureList::IsEnabled(
          features::kNavigationThreadingOptimizations)) {
    content_settings::ContentSettingsManagerImpl::Create(
        render_process_host,
        content_settings_manager.InitWithNewPipeAndPassReceiver(),
        std::make_unique<chrome::ContentSettingsManagerDelegate>());
  }
  renderer_configuration->SetInitialConfiguration(
      is_off_the_record_, std::move(chromeos_listener_receiver),
      std::move(content_settings_manager));

  renderer_configuration->SetConfiguration(CreateRendererDynamicParams());

  RendererContentSettingRules rules;
  if (render_process_host->IsForGuestsOnly()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    GetGuestViewDefaultContentSettingRules(is_off_the_record_, &rules);
#else
    NOTREACHED();
#endif
  } else {
    content_settings::GetRendererContentSettingRules(host_content_settings_map_,
                                                     &rules);

    // Always allow scripting in PDF renderers to retain the functionality of
    // the scripted messaging proxy in between the plugins in the PDF renderers
    // and the PDF extension UI. Content settings for JavaScript embedded in
    // PDFs are enforced by the PDF plugin.
    if (render_process_host->IsPdf()) {
      rules.script_rules.clear();
      rules.script_rules.emplace_back(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::Wildcard(),
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
          std::string(), is_off_the_record_);
    }
  }
  renderer_configuration->SetContentSettingRules(rules);
}

RendererUpdater::RendererConfigurations
RendererUpdater::GetRendererConfigurations() {
  RendererConfigurations rc;
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    content::RenderProcessHost* render_process_host = it.GetCurrentValue();
    Profile* renderer_profile =
        Profile::FromBrowserContext(render_process_host->GetBrowserContext());
    if (renderer_profile == profile_) {
      auto renderer_configuration =
          GetRendererConfiguration(render_process_host);
      if (renderer_configuration)
        rc.push_back(std::make_pair(render_process_host,
                                    std::move(renderer_configuration)));
    }
  }
  return rc;
}

mojo::AssociatedRemote<chrome::mojom::RendererConfiguration>
RendererUpdater::GetRendererConfiguration(
    content::RenderProcessHost* render_process_host) {
  IPC::ChannelProxy* channel = render_process_host->GetChannel();
  if (!channel)
    return mojo::AssociatedRemote<chrome::mojom::RendererConfiguration>();

  mojo::AssociatedRemote<chrome::mojom::RendererConfiguration>
      renderer_configuration;
  channel->GetRemoteAssociatedInterface(&renderer_configuration);
  return renderer_configuration;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void RendererUpdater::OnSessionRestoreStateChanged(
    Profile* user_profile,
    ash::OAuth2LoginManager::SessionRestoreState state) {
  merge_session_running_ =
      ash::merge_session_throttling_utils::ShouldDelayRequestForProfile(
          original_profile_);
  if (merge_session_running_)
    return;

  for (auto& chromeos_listener : chromeos_listeners_)
    chromeos_listener->MergeSessionComplete();
  chromeos_listeners_.clear();
}
#endif

void RendererUpdater::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSync) ==
      signin::PrimaryAccountChangeEvent::Type::kNone) {
    return;
  }
  UpdateAllRenderers(kUpdateDynamicParams);
}

void RendererUpdater::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  // Do not send updates for non-renderer supported types.
  if (!IsRendererSupportedContentSettingsTypeSet(content_type_set)) {
    return;
  }

  // Send updates to all RenderProcessHosts.
  UpdateAllRenderers(kUpdateContentSettings);
}

void RendererUpdater::UpdateAllRenderers(UpdateTypes update_types) {
  chrome::mojom::DynamicParamsPtr dynamic_params;
  if (update_types & kUpdateDynamicParams) {
    dynamic_params = CreateRendererDynamicParams();
  }

  RendererContentSettingRules rules;
  if (update_types & kUpdateContentSettings) {
    content_settings::GetRendererContentSettingRules(host_content_settings_map_,
                                                     &rules);
  }

  auto renderer_configurations = GetRendererConfigurations();
  for (auto& renderer_configuration : renderer_configurations) {
    content::RenderProcessHost* render_process_host =
        renderer_configuration.first;
    if (!render_process_host->IsInitializedAndNotDead())
      continue;

    if (update_types & kUpdateDynamicParams) {
      renderer_configuration.second->SetConfiguration(dynamic_params.Clone());
    }
    if (update_types & kUpdateContentSettings &&
        ShouldSendUpdatedContentSettingsRulesToRenderer(render_process_host)) {
      renderer_configuration.second->SetContentSettingRules(rules);
    }
  }
}

chrome::mojom::DynamicParamsPtr RendererUpdater::CreateRendererDynamicParams()
    const {
  return chrome::mojom::DynamicParams::New(
      force_google_safesearch_.GetValue(), force_youtube_restrict_.GetValue(),
      allowed_domains_for_apps_.GetValue());
}
