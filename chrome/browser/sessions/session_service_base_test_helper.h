// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_TEST_HELPER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_TEST_HELPER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/sessions/core/session_id.h"

class SessionServiceBase;

namespace base {
class Location;
class SequencedTaskRunner;
}  // namespace base

namespace sessions {
class SerializedNavigationEntry;
struct SerializedUserAgentOverride;
struct SessionTab;
struct SessionWindow;
}  // namespace sessions

// A simple class that makes writing SessionService related tests easier.

class SessionServiceBaseTestHelper {
 public:
  void SaveNow();

  void PrepareTabInWindow(SessionID window_id,
                          SessionID tab_id,
                          int visual_index,
                          bool select);

  void SetTabExtensionAppID(SessionID window_id,
                            SessionID tab_id,
                            const std::string& extension_app_id);

  void SetTabUserAgentOverride(
      SessionID window_id,
      SessionID tab_id,
      const sessions::SerializedUserAgentOverride& user_agent_override);

  // Reads the contents of the last session.
  void ReadWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      SessionID* active_window_id);

  void AssertTabEquals(SessionID window_id,
                       SessionID tab_id,
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

  void SetServiceBase(SessionServiceBase* service);
  SessionServiceBase* service_base() { return service_; }

  void RunTaskOnBackendThread(const base::Location& from_here,
                              base::OnceClosure task);

  scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner();

  void SetAvailableRange(SessionID tab_id, const std::pair<int, int>& range);
  bool GetAvailableRange(SessionID tab_id, std::pair<int, int>* range);

 protected:
  explicit SessionServiceBaseTestHelper(SessionServiceBase* base);

 private:
  raw_ptr<SessionServiceBase, DanglingUntriaged> service_;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_TEST_HELPER_H_
