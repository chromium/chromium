// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_ON_TASK_LOCKED_QUIZ_SESSION_MANAGER_H_
#define CHROME_BROWSER_ASH_BOCA_ON_TASK_LOCKED_QUIZ_SESSION_MANAGER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace ash::boca {

// Manages the lifecycle of a locked quiz session within the Boca SWA.
class ASH_EXPORT LockedQuizSessionManager : public KeyedService {
 public:
  using CreateWindowCompletionCallback =
      base::OnceCallback<void(const SessionID&)>;

  explicit LockedQuizSessionManager(content::BrowserContext* context);
  LockedQuizSessionManager(const LockedQuizSessionManager&) = delete;
  LockedQuizSessionManager& operator=(const LockedQuizSessionManager&) = delete;
  ~LockedQuizSessionManager() override;

  // Open the quiz with the given url in Boca SWA window, and lock the Boca SWA.
  void OpenLockedQuiz(const GURL& quiz_url,
                      CreateWindowCompletionCallback callback);

  // This function sets the state of the browser window to a "locked"
  // fullscreen state (where the user can't exit fullscreen) in response to a
  // call to either `chrome.windows.create` or `chrome.windows.update` when the
  // screen is set locked. This is only necessary for ChromeOS and is
  // restricted to allowlisted extensions.
  void SetLockedFullscreenState(Browser* browser, bool pinned);

 private:
  // Callback invoked after the SWA launch attempt.
  void OnBocaSWALaunched(const GURL& quiz_url,
                         CreateWindowCompletionCallback callback,
                         bool success);

  const raw_ptr<Profile> profile_;
  const std::unique_ptr<OnTaskSystemWebAppManager> system_web_app_manager_;

  base::WeakPtrFactory<LockedQuizSessionManager> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROME_BROWSER_ASH_BOCA_ON_TASK_LOCKED_QUIZ_SESSION_MANAGER_H_
