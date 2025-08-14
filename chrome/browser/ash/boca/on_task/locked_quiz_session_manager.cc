// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/locked_quiz_session_manager.h"

#include <memory>

#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace ash::boca {
namespace {

using ::boca::LockedNavigationOptions;

// Returns a pointer to the browser window with the specified id. Returns
// nullptr if there is no match.
Browser* GetBrowserWindowWithID(SessionID window_id) {
  if (!window_id.is_valid()) {
    return nullptr;
  }
  Browser* result = nullptr;
  ash::BrowserController::GetInstance()->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingCreationTime,
      [&](ash::BrowserDelegate& browser) {
        if (browser.GetSessionID() == window_id) {
          result = &browser.GetBrowser();
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
    std::move(callback).Run(nullptr);
    return;
  }

  const SessionID window_id =
      system_web_app_manager_->GetActiveSystemWebAppWindowID();
  if (!window_id.is_valid()) {
    LOG(WARNING)
        << "Could not find a valid window ID for the launched Boca SWA.";
    std::move(callback).Run(nullptr);
    return;
  }

  // Lock Boca SWA window and apply navigation restrictions for the quiz.
  system_web_app_manager_->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                            window_id);
  system_web_app_manager_->SetWindowTrackerForSystemWebAppWindow(window_id, {});
  system_web_app_manager_->SetParentTabsRestriction(
      window_id, LockedNavigationOptions::DOMAIN_NAVIGATION);

  auto* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    LOG(WARNING) << "Successfully configured Boca SWA window but could not "
                 << "find its Browser instance for window_id: " << window_id;
  }
  std::move(callback).Run(browser);
}

}  // namespace ash::boca
