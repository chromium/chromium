// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/fullscreen_controller_client_lacros.h"

#include "base/functional/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/ui/wm/fullscreen/keep_fullscreen_for_url_checker.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "url/gurl.h"

FullscreenControllerClientLacros::FullscreenControllerClientLacros() {
  auto* const lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::FullscreenController>()) {
    lacros_service->GetRemote<crosapi::mojom::FullscreenController>()
        ->AddClient(receiver_.BindNewPipeAndPassRemote());
  }
}

FullscreenControllerClientLacros::~FullscreenControllerClientLacros() = default;

void FullscreenControllerClientLacros::ShouldExitFullscreenBeforeLock(
    base::OnceCallback<void(bool)> callback) {
  if (!keep_fullscreen_checker_) {
    keep_fullscreen_checker_ =
        std::make_unique<chromeos::KeepFullscreenForUrlChecker>(
            ProfileManager::GetPrimaryUserProfile()->GetPrefs());
  }

  if (!keep_fullscreen_checker_
           ->IsKeepFullscreenWithoutNotificationPolicySet()) {
    std::move(callback).Run(/*should_exit_fullscreen=*/true);
    return;
  }

  // Get the web content if the active window is a browser window.
  content::WebContents* web_contents = nullptr;
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  if (browser) {
    web_contents = browser->tab_strip_model()->GetActiveWebContents();
  }

  // Get the web content if the active window is an app window.
  if (!web_contents) {
    web_contents = GetActiveAppWindowWebContents();
  }

  if (!web_contents) {
    std::move(callback).Run(/*should_exit_fullscreen=*/true);
    return;
  }

  // Check if it is allowed by user pref to keep full screen for the window URL.
  GURL url = web_contents->GetLastCommittedURL();
  std::move(callback).Run(
      keep_fullscreen_checker_->ShouldExitFullscreenForUrl(url));
}

content::WebContents*
FullscreenControllerClientLacros::GetActiveAppWindowWebContents() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile) {
    return nullptr;
  }

  const auto& app_windows =
      extensions::AppWindowRegistry::Get(profile)->app_windows();
  for (extensions::AppWindow* app_window : app_windows) {
    if (app_window->GetBaseWindow()->IsActive()) {
      return app_window->web_contents();
    }
  }

  return nullptr;
}
