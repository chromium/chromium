// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_
#define CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/renderer_configuration.mojom-forward.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/signin/oauth2_login_manager.h"
#endif

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
class BoundSessionCookieRefreshService;
#endif

class Profile;

namespace content {
class RenderProcessHost;
}

// The RendererUpdater is responsible for updating renderers about state change.
class RendererUpdater : public KeyedService
#if BUILDFLAG(IS_CHROMEOS)
    ,
                        public ash::OAuth2LoginManager::Observer
#endif
{
 public:
  explicit RendererUpdater(Profile* profile);
  RendererUpdater(const RendererUpdater&) = delete;
  RendererUpdater& operator=(const RendererUpdater&) = delete;
  ~RendererUpdater() override;

  // KeyedService:
  void Shutdown() override;

  // Initialize a newly-started renderer process.
  void InitializeRenderer(content::RenderProcessHost* render_process_host);

 private:
  using RendererConfigurations = std::vector<
      std::pair<content::RenderProcessHost*,
                mojo::AssociatedRemote<chrome::mojom::RendererConfiguration>>>;

  // Returns active mojo interfaces to RendererConfiguration endpoints.
  RendererConfigurations GetRendererConfigurations();

  mojo::AssociatedRemote<chrome::mojom::RendererConfiguration>
  GetRendererConfiguration(content::RenderProcessHost* render_process_host);

#if BUILDFLAG(IS_CHROMEOS)
  // ash::OAuth2LoginManager::Observer:
  void OnSessionRestoreStateChanged(
      Profile* user_profile,
      ash::OAuth2LoginManager::SessionRestoreState state) override;
#endif

  // Update all renderers due to a configuration change.
  void UpdateAllRenderers();

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  // Creates bound session throttler parameters that are subset of the dynamic
  // renderer parameters.
  std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
  GetBoundSessionThrottlerParams() const;
#endif

  // Create renderer configuration that changes at runtime.
  chrome::mojom::DynamicParamsPtr CreateRendererDynamicParams() const;

  const raw_ptr<Profile> profile_;
  const bool is_off_the_record_;
  const raw_ptr<Profile> original_profile_;

#if BUILDFLAG(IS_CHROMEOS)
  raw_ptr<ash::OAuth2LoginManager> oauth2_login_manager_;
  bool merge_session_running_;
  std::vector<mojo::Remote<chrome::mojom::ChromeOSListener>>
      chromeos_listeners_;
#endif
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  const raw_ptr<BoundSessionCookieRefreshService>
      bound_session_cookie_refresh_service_ = nullptr;
#endif

  PrefChangeRegistrar pref_change_registrar_;

  // Prefs that we sync to the renderers.
  BooleanPrefMember force_google_safesearch_;
  IntegerPrefMember force_youtube_restrict_;
  StringPrefMember allowed_domains_for_apps_;
};

#endif  // CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_
