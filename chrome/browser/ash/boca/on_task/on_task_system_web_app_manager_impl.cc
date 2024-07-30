// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chromeos/window_pin_util.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

OnTaskSystemWebAppManagerImpl::OnTaskSystemWebAppManagerImpl(Profile* profile)
    : profile_(profile) {}

OnTaskSystemWebAppManagerImpl::~OnTaskSystemWebAppManagerImpl() = default;

void OnTaskSystemWebAppManagerImpl::LaunchSystemWebAppAsync(
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::LaunchSystemWebAppAsync(
      profile_, SystemWebAppType::BOCA, SystemAppLaunchParams(),
      /*window_info=*/nullptr,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             apps::LaunchResult&& launch_result) {
            std::move(callback).Run(launch_result.state ==
                                    apps::LaunchResult::State::kSuccess);
          },
          std::move(callback)));
}

void OnTaskSystemWebAppManagerImpl::CloseActiveSystemWebAppWindow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Browser* const browser =
      FindSystemWebAppBrowser(profile_, SystemWebAppType::BOCA);
  if (browser) {
    browser->TryToCloseWindow(/*skip_beforeunload=*/true, base::DoNothing());
  }
}

bool OnTaskSystemWebAppManagerImpl::HasActiveSystemWebAppWindow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Browser* const browser =
      FindSystemWebAppBrowser(profile_, SystemWebAppType::BOCA);
  return (browser != nullptr);
}

void OnTaskSystemWebAppManagerImpl::SetPinStateForActiveSystemWebAppWindow(
    bool pinned) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Browser* const browser =
      FindSystemWebAppBrowser(profile_, SystemWebAppType::BOCA);
  if (!browser) {
    return;
  }
  aura::Window* const native_window = browser->window()->GetNativeWindow();
  bool currently_pinned = IsWindowPinned(native_window);
  if (pinned == currently_pinned) {
    // Nothing to do.
    return;
  }
  if (pinned) {
    PinWindow(native_window, /*trusted=*/true);
    browser->command_controller()->LockedFullscreenStateChanged();
  } else {
    UnpinWindow(native_window);
    browser->command_controller()->LockedFullscreenStateChanged();
  }
}

}  // namespace ash
