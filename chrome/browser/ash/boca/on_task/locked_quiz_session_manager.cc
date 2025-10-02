// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/locked_quiz_session_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/window_pin_util.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "content/public/browser/browser_context.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace ash::boca {
namespace {

using ::boca::LockedNavigationOptions;

// Returns a pointer to the browser delegate for the window with the specified
// id. Returns nullptr if there is no match.
ash::BrowserDelegate* GetBrowserDelegateBySessionID(SessionID session_id) {
  if (!session_id.is_valid()) {
    return nullptr;
  }
  ash::BrowserDelegate* result = nullptr;
  ash::BrowserController::GetInstance()->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingCreationTime,
      [&](ash::BrowserDelegate& browser) {
        if (browser.GetSessionID() == session_id) {
          result = &browser;
          return ash::BrowserController::kBreakIteration;
        }
        return ash::BrowserController::kContinueIteration;
      });
  return result;
}
}  // namespace

LockedQuizSessionManager::LockedQuizSessionManager(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      system_web_app_manager_(
          std::make_unique<OnTaskSystemWebAppManagerImpl>(profile_)) {}

LockedQuizSessionManager::~LockedQuizSessionManager() = default;

void LockedQuizSessionManager::OpenLockedQuiz(
    const GURL& quiz_url,
    CreateWindowCompletionCallback callback) {
  // TODO(crbug.com/436601346): Support opening up the browser instance for
  // locked quiz.
  const SessionID window_id =
      system_web_app_manager_->GetActiveSystemWebAppWindowID();
  // TODO(crbug.com/433353578): Support opening the locked quiz in Class Tools.
  if (window_id.is_valid()) {
    // Unlock and close the pre-existing Boca window.
    system_web_app_manager_->SetPinStateForSystemWebAppWindow(/*pinned=*/false,
                                                              window_id);
    system_web_app_manager_->CloseSystemWebAppWindow(window_id);
  }
  system_web_app_manager_->LaunchSystemWebAppAsync(
      base::BindOnce(&LockedQuizSessionManager::OnBocaSWALaunched,
                     weak_ptr_factory_.GetWeakPtr(), quiz_url,
                     std::move(callback)),
      quiz_url);
}

void LockedQuizSessionManager::OnBocaSWALaunched(
    const GURL& quiz_url,
    CreateWindowCompletionCallback callback,
    bool success) {
  if (!success) {
    LOG(WARNING) << "Boca SWA launch failed. Cannot start locked quiz session.";
    // TODO(crbug.com/435523242): Add UMA metric here to record Boca launch
    // failure for locked quiz.
    std::move(callback).Run(SessionID::InvalidValue());
    return;
  }

  const SessionID window_id =
      system_web_app_manager_->GetActiveSystemWebAppWindowID();
  if (!window_id.is_valid()) {
    LOG(WARNING)
        << "Could not find a valid window ID for the launched Boca SWA.";
    std::move(callback).Run(SessionID::InvalidValue());
    return;
  }

  // Prepare SWA window. We do not close bundle content to ensure we do not
  // inadvertently close the locked quiz tab if there are other tabs opened by
  // extensions on launch.
  system_web_app_manager_->PrepareSystemWebAppWindowForOnTask(
      window_id, /*close_bundle_content=*/false);

  // Lock Boca SWA window and apply navigation restrictions for the quiz.
  system_web_app_manager_->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                            window_id);
  system_web_app_manager_->SetWindowTrackerForSystemWebAppWindow(window_id, {});
  system_web_app_manager_->SetParentTabsRestriction(
      window_id, LockedNavigationOptions::DOMAIN_NAVIGATION);

  auto* const browser_delegate = GetBrowserDelegateBySessionID(window_id);
  LOG_IF(WARNING, !browser_delegate)
      << "Successfully configured Boca SWA window but could not "
      << "find its Browser instance for window_id: " << window_id;

  // Activate SWA window to ensure it remains the active/focused window.
  if (browser_delegate) {
    browser_delegate->Activate();
  }
  std::move(callback).Run(window_id);
}

void LockedQuizSessionManager::SetLockedFullscreenState(Browser* browser,
                                                        bool pinned) {
  if (ash::features::IsBocaOnTaskLockedQuizMigrationEnabled()) {
    bool is_boca_app_instance =
        ash::IsBrowserForSystemWebApp(browser, ash::SystemWebAppType::BOCA);

    // Enforce that the browser instance provided is the Boca SWA.
    if (!is_boca_app_instance) {
      LOG(ERROR) << "Attempted to lock a browser window that is not the "
                 << "Boca SWA.";
      // TODO(crbug.com/441365121): Return false to surface the error back to
      // the extension.
      return;
    }

    const SessionID browser_window_id = browser->session_id();
    system_web_app_manager_->SetPinStateForSystemWebAppWindow(
        pinned, browser_window_id);
    return;
  }

  // TODO(crbug.com/438540029): Clean up after migrating locked quizzes to use
  // the Boca SWA.
  aura::Window* const window = browser->window()->GetNativeWindow();
  DCHECK(window);

  CHECK_NE(GetWindowPinType(window), chromeos::WindowPinType::kPinned)
      << "Extensions only set Trusted Pinned";

  // As this gets triggered from extensions, we might encounter this case.
  if (IsWindowPinned(window) == pinned) {
    return;
  }

  if (pinned) {
    // Pins from extension are always trusted.
    PinWindow(window, /*trusted=*/true);
  } else {
    UnpinWindow(window);
  }

  // Update the set of available browser commands.
  browser->command_controller()->LockedFullscreenStateChanged();
}

}  // namespace ash::boca
