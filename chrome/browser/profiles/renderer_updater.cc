// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/renderer_updater.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/content_settings_manager_delegate.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/common/content_settings_manager.mojom.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/signin/merge_session_throttling_utils.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager_factory.h"
#endif

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

RendererUpdater::RendererUpdater(Profile* profile)
    : profile_(profile),
      is_off_the_record_(profile_->IsOffTheRecord()),
      original_profile_(profile->GetOriginalProfile())
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      bound_session_cookie_refresh_service_(
          BoundSessionCookieRefreshServiceFactory::GetForProfile(profile))
#endif
{
#if BUILDFLAG(IS_CHROMEOS)
  oauth2_login_manager_ =
      ash::OAuth2LoginManagerFactory::GetForProfile(original_profile_);
  oauth2_login_manager_->AddObserver(this);
  merge_session_running_ =
      ash::merge_session_throttling_utils::ShouldDelayRequestForProfile(
          original_profile_);
#endif

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (bound_session_cookie_refresh_service_) {
    // `base::Unretained` is safe as `this` deregister itself on destruction.
    bound_session_cookie_refresh_service_
        ->SetRendererBoundSessionThrottlerParamsUpdaterDelegate(
            base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                                base::Unretained(this)));
  }
#endif

  PrefService* pref_service = profile_->GetPrefs();
  force_google_safesearch_.Init(policy::policy_prefs::kForceGoogleSafeSearch,
                                pref_service);
  force_youtube_restrict_.Init(policy::policy_prefs::kForceYouTubeRestrict,
                               pref_service);
  allowed_domains_for_apps_.Init(prefs::kAllowedDomainsForApps, pref_service);

  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      policy::policy_prefs::kForceGoogleSafeSearch,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      policy::policy_prefs::kForceYouTubeRestrict,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kAllowedDomainsForApps,
      base::BindRepeating(&RendererUpdater::UpdateAllRenderers,
                          base::Unretained(this)));
}

RendererUpdater::~RendererUpdater() {
#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(!oauth2_login_manager_);
#endif
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (bound_session_cookie_refresh_service_) {
    bound_session_cookie_refresh_service_
        ->SetRendererBoundSessionThrottlerParamsUpdaterDelegate(
            base::RepeatingClosure());
  }
#endif
}

void RendererUpdater::Shutdown() {
  pref_change_registrar_.RemoveAll();
#if BUILDFLAG(IS_CHROMEOS)
  oauth2_login_manager_->RemoveObserver(this);
  oauth2_login_manager_ = nullptr;
#endif
}

void RendererUpdater::InitializeRenderer(
    content::RenderProcessHost* render_process_host) {
  DCHECK_EQ(profile_, Profile::FromBrowserContext(
                          render_process_host->GetBrowserContext()));
  auto renderer_configuration = GetRendererConfiguration(render_process_host);

  mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
      chromeos_listener_receiver;
#if BUILDFLAG(IS_CHROMEOS)
  if (merge_session_running_) {
    mojo::Remote<chrome::mojom::ChromeOSListener> chromeos_listener;
    chromeos_listener_receiver = chromeos_listener.BindNewPipeAndPassReceiver();
    chromeos_listeners_.push_back(std::move(chromeos_listener));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  mojo::PendingRemote<content_settings::mojom::ContentSettingsManager>
      content_settings_manager;
  content_settings::ContentSettingsManagerImpl::Create(
      render_process_host,
      content_settings_manager.InitWithNewPipeAndPassReceiver(),
      std::make_unique<ContentSettingsManagerDelegate>());
  mojo::PendingRemote<chrome::mojom::BoundSessionRequestThrottledHandler>
      bound_session_request_throttled_handler;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (bound_session_cookie_refresh_service_) {
    bound_session_cookie_refresh_service_
        ->AddBoundSessionRequestThrottledHandlerReceiver(
            bound_session_request_throttled_handler
                .InitWithNewPipeAndPassReceiver());
  }
#endif
  renderer_configuration->SetInitialConfiguration(
      is_off_the_record_, std::move(chromeos_listener_receiver),
      std::move(content_settings_manager),
      std::move(bound_session_request_throttled_handler));

  renderer_configuration->SetConfiguration(CreateRendererDynamicParams());
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

#if BUILDFLAG(IS_CHROMEOS)
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

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
RendererUpdater::GetBoundSessionThrottlerParams() const {
  if (bound_session_cookie_refresh_service_) {
    return bound_session_cookie_refresh_service_
        ->GetBoundSessionThrottlerParams();
  }
  return {};
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

void RendererUpdater::UpdateAllRenderers() {
  chrome::mojom::DynamicParamsPtr dynamic_params =
      CreateRendererDynamicParams();
  auto renderer_configurations = GetRendererConfigurations();
  for (auto& renderer_configuration : renderer_configurations) {
    content::RenderProcessHost* render_process_host =
        renderer_configuration.first;
    if (!render_process_host->IsInitializedAndNotDead()) {
      continue;
    }
    renderer_configuration.second->SetConfiguration(dynamic_params.Clone());
  }
}

chrome::mojom::DynamicParamsPtr RendererUpdater::CreateRendererDynamicParams()
    const {
  return chrome::mojom::DynamicParams::New(
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      GetBoundSessionThrottlerParams(),
#endif
      force_google_safesearch_.GetValue(), force_youtube_restrict_.GetValue(),
      allowed_domains_for_apps_.GetValue());
}
