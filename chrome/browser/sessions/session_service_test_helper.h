// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_TEST_HELPER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_TEST_HELPER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/sessions/session_service.h"
#include "components/sessions/core/session_id.h"

class Profile;
class SessionService;

namespace base {
class Location;
class SequencedTaskRunner;
}

namespace sessions {
class SerializedNavigationEntry;
struct SerializedUserAgentOverride;
struct SessionTab;
struct SessionWindow;
}

// A simple class that makes writing SessionService related tests easier.

class SessionServiceTestHelper {
 public:
  SessionServiceTestHelper();
  explicit SessionServiceTestHelper(Profile* profile);
  explicit SessionServiceTestHelper(SessionService* service);
  SessionServiceTestHelper(const SessionServiceTestHelper&) = delete;
  SessionServiceTestHelper& operator=(const SessionServiceTestHelper&) = delete;
  ~SessionServiceTestHelper();

  void SaveNow();

  void PrepareTabInWindow(const SessionID& window_id,
                          const SessionID& tab_id,
                          int visual_index,
                          bool select);

  void SetTabExtensionAppID(const SessionID& window_id,
                            const SessionID& tab_id,
                            const std::string& extension_app_id);

  void SetTabUserAgentOverride(
      const SessionID& window_id,
      const SessionID& tab_id,
      const sessions::SerializedUserAgentOverride& user_agent_override);

  void SetForceBrowserNotAliveWithNoWindows(
      bool force_browser_not_alive_with_no_windows);

  // Reads the contents of the last session.
  void ReadWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      SessionID* active_window_id);

  void AssertTabEquals(const SessionID& window_id,
                       const SessionID& tab_id,
                       int visual_index,
                       int nav_index,
                       size_t nav_count,
                       const sessions::SessionTab& session_tab);

  void AssertTabEquals(int visual_index,
                       int nav_index,
                       size_t nav_count,
                       const sessions::SessionTab& session_tab);

  void AssertNavigationEquals(
      const sessions::SerializedNavigationEntry& expected,
      const sessions::SerializedNavigationEntry& actual);

  void AssertSingleWindowWithSingleTab(
      const std::vector<std::unique_ptr<sessions::SessionWindow>>& windows,
      size_t nav_count);

  void SetService(SessionService* service);
  SessionService* service() { return service_; }

  void RunTaskOnBackendThread(const base::Location& from_here,
                              base::OnceClosure task);

  scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner();

  void SetAvailableRange(const SessionID& tab_id,
                         const std::pair<int, int>& range);
  bool GetAvailableRange(const SessionID& tab_id, std::pair<int, int>* range);

  void SetHasOpenTrackableBrowsers(bool has_open_trackable_browsers);
  bool GetHasOpenTrackableBrowsers();

  void SetIsOnlyOneTabLeft(bool is_only_one_tab_left);

  bool HasPendingReset();

  bool HasPendingSave();

  void SetSavingEnabled(bool enabled) { service_->SetSavingEnabled(enabled); }

  bool did_save_commands_at_least_once() const {
    return service_->did_save_commands_at_least_once_;
  }

  sessions::CommandStorageManager* command_storage_manager() {
    return service_->command_storage_manager_.get();
  }

 private:
  raw_ptr<SessionService> service_;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_TEST_HELPER_H_
