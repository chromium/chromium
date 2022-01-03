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
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
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
}  // namespace

RendererUpdater::RendererUpdater(Profile* profile) : profile_(profile) {
  identity_manager_ = IdentityManagerFactory::GetForProfile(profile);
  identity_manager_observation_.Observe(identity_manager_.get());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  oauth2_login_manager_ =
      ash::OAuth2LoginManagerFactory::GetForProfile(profile_);
  oauth2_login_manager_->AddObserver(this);
  merge_session_running_ =
      ash::merge_session_throttling_utils::ShouldDelayRequestForProfile(
          profile_);
#endif

  PrefService* pref_service = profile->GetPrefs();
  force_google_safesearch_.Init(prefs::kForceGoogleSafeSearch, pref_service);
  force_youtube_restrict_.Init(prefs::kForceYouTubeRestrict, pref_service);
  allowed_domains_for_apps_.Init(prefs::kAllowedDomainsForApps, pref_service);

  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kForceGoogleSafeSearch,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kForceYouTubeRestrict,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kAllowedDomainsForApps,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this)));
}

RendererUpdater::~RendererUpdater() {
  DCHECK(!identity_manager_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(!oauth2_login_manager_);
#endif
}

void RendererUpdater::Shutdown() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  oauth2_login_manager_->RemoveObserver(this);
  oauth2_login_manager_ = nullptr;
#endif
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
}

void RendererUpdater::InitializeRenderer(
    content::RenderProcessHost* render_process_host) {
  auto renderer_configuration = GetRendererConfiguration(render_process_host);

  Profile* profile =
      Profile::FromBrowserContext(render_process_host->GetBrowserContext());
  bool is_incognito_process = profile->IsOffTheRecord();

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
      is_incognito_process, std::move(chromeos_listener_receiver),
      std::move(content_settings_manager));

  UpdateRenderer(&renderer_configuration);

  RendererContentSettingRules rules;
  if (render_process_host->IsForGuestsOnly()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    GetGuestViewDefaultContentSettingRules(is_incognito_process, &rules);
#else
    NOTREACHED();
#endif
  } else {
    content_settings::GetRendererContentSettingRules(
        HostContentSettingsMapFactory::GetForProfile(profile), &rules);

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
          std::string(), is_incognito_process);
    }
  }
  renderer_configuration->SetContentSettingRules(rules);
}

std::vector<mojo::AssociatedRemote<chrome::mojom::RendererConfiguration>>
RendererUpdater::GetRendererConfigurations() {
  std::vector<mojo::AssociatedRemote<chrome::mojom::RendererConfiguration>> rv;
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    Profile* renderer_profile =
        static_cast<Profile*>(it.GetCurrentValue()->GetBrowserContext());
    if (renderer_profile == profile_ ||
        renderer_profile->GetOriginalProfile() == profile_) {
      auto renderer_configuration =
          GetRendererConfiguration(it.GetCurrentValue());
      if (renderer_configuration)
        rv.push_back(std::move(renderer_configuration));
    }
  }
  return rv;
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
          profile_);
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
  UpdateAllRenderers();
}

void RendererUpdater::UpdateAllRenderers() {
  auto renderer_configurations = GetRendererConfigurations();
  for (auto& renderer_configuration : renderer_configurations)
    UpdateRenderer(&renderer_configuration);
}

void RendererUpdater::UpdateRenderer(
    mojo::AssociatedRemote<chrome::mojom::RendererConfiguration>*
        renderer_configuration) {
  (*renderer_configuration)
      ->SetConfiguration(chrome::mojom::DynamicParams::New(
          force_google_safesearch_.GetValue(),
          force_youtube_restrict_.GetValue(),
          allowed_domains_for_apps_.GetValue()));
}
