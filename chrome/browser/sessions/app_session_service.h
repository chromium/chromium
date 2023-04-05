// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_APP_SESSION_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_APP_SESSION_SERVICE_H_

#include "chrome/browser/sessions/session_service_base.h"

class Profile;

namespace sessions {
struct SessionWindow;
}  // namespace sessions

// AppSessionService ---------------------------------------------------------
// AppSessionService handles session data for apps. It is a sibling of the
// SessionService class which is also based on SessionServiceBase.

// Primarily, the differences between SessionService and this class is that
// AppSessionService does not track things like TabGroups and other things
// that only apply to browser windows.

// In the future, if additional features are required by app restores, those
// features can be moved to the SessionServiceBase and shared by both child
// classes.
class AppSessionService : public SessionServiceBase {
 public:
  explicit AppSessionService(Profile* profile);
  AppSessionService(const AppSessionService&) = delete;
  AppSessionService& operator=(const AppSessionService&) = delete;
  ~AppSessionService() override;

  // SessionServiceBase:
  void TabClosed(SessionID window_id, SessionID tab_id) override;
  void WindowOpened(Browser* browser) override;
  void WindowClosing(SessionID window_id) override;
  void WindowClosed(SessionID window_id) override;
  void SetWindowType(SessionID window_id, Browser::Type type) override;
  Browser::Type GetDesiredBrowserTypeForWebContents() override;
  bool ShouldRestoreWindowOfType(
      sessions::SessionWindow::WindowType window_type) const override;
  void ScheduleResetCommands() override;
  void RebuildCommandsIfRequired() override;

 private:
  base::WeakPtrFactory<AppSessionService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SESSIONS_APP_SESSION_SERVICE_H_
