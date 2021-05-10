// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_
#define CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_

#include <string>
#include <vector>

#include "base/scoped_observer.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/renderer_configuration.mojom-forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/signin/oauth2_login_manager.h"
#endif

class Profile;

namespace content {
class RenderProcessHost;
}

// The RendererUpdater is responsible for updating renderers about state change.
class RendererUpdater : public KeyedService,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                        public chromeos::OAuth2LoginManager::Observer,
#endif
                        public signin::IdentityManager::Observer {
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
  std::vector<mojo::AssociatedRemote<chrome::mojom::RendererConfiguration>>
  GetRendererConfigurations();

  mojo::AssociatedRemote<chrome::mojom::RendererConfiguration>
  GetRendererConfiguration(content::RenderProcessHost* render_process_host);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // chromeos::OAuth2LoginManager::Observer:
  void OnSessionRestoreStateChanged(
      Profile* user_profile,
      chromeos::OAuth2LoginManager::SessionRestoreState state) override;
#endif

  // IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  // Update all renderers due to a configuration change.
  void UpdateAllRenderers();

  // Update the given renderer due to a configuration change.
  void UpdateRenderer(
      mojo::AssociatedRemote<chrome::mojom::RendererConfiguration>*
          renderer_configuration);

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::OAuth2LoginManager* oauth2_login_manager_;
  bool merge_session_running_;
  std::vector<mojo::Remote<chrome::mojom::ChromeOSListener>>
      chromeos_listeners_;
#endif

  // Prefs that we sync to the renderers.
  BooleanPrefMember force_google_safesearch_;
  IntegerPrefMember force_youtube_restrict_;
  StringPrefMember allowed_domains_for_apps_;

  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_;
  signin::IdentityManager* identity_manager_;
};

#endif  // CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_
