// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SESSIONS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SESSIONS_HELPER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/synced_session.h"
#include "ui/base/window_open_disposition.h"

class GURL;

namespace sessions_helper {

using SyncedSessionVector = std::vector<
    raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>;
using ScopedWindowMap =
    std::map<SessionID, std::unique_ptr<sync_sessions::SyncedSessionWindow>>;

// Copies the local session windows of profile at |index| to |local_windows|.
// Returns true if successful.
bool GetLocalWindows(int browser_index, ScopedWindowMap* local_windows);

// Checks that window count and foreign session count are 0.
bool CheckInitialState(int browser_index);

// Returns number of open windows for a profile.
int GetNumWindows(int browser_index);

// Returns number of foreign sessions for a profile.
int GetNumForeignSessions(int browser_index);

// Fills the sessions vector with the SyncableService's foreign session data.
// Caller owns |sessions|, but not SyncedSessions objects within.
// Returns true if foreign sessions were found, false otherwise.
bool GetSessionData(int browser_index, SyncedSessionVector* sessions);

// Compares two tab navigations base on the parameters we sync.
// (Namely, we don't sync state or type mask)
bool NavigationEquals(const sessions::SerializedNavigationEntry& expected,
                      const sessions::SerializedNavigationEntry& actual);

// Verifies that two SessionWindows match.
// Returns:
//  - true if all the following match:
//    1. number of SessionWindows,
//    2. number of tabs per SessionWindow,
//    3. number of tab navigations per tab,
//    4. actual tab navigations contents
// - false otherwise.
bool WindowsMatch(const ScopedWindowMap& win1, const ScopedWindowMap& win2);

// Opens (appends) a single tab  in the browser at |index| and block until the
// sessions bridge is aware of it. Returns true upon success, false otherwise.
bool OpenTab(int browser_index, const GURL& url);

// See OpenTab, except that the tab is opened in position |tab_index|.
// If |tab_index| is -1 or greater than the number of tabs, the tab will be
// appended to the end of the strip. i.e. if tab_index is 3 for a tab strip of
// size 1, the new tab will be in position 1.
bool OpenTabAtIndex(int browser_index, int tab_index, const GURL& url);

// Opens multiple tabs and blocks until the sessions bridge is aware of all of
// them. Returns true on success, false on failure.
bool OpenMultipleTabs(int browser_index, const std::vector<GURL>& urls);

// Closes the tab |tab_index| in the browser at |index|.
void CloseTab(int browser_index, int tab_index);

// Moves the tab in position |tab_index| in the TabStrip for browser at
// |from_browser_index| to the TabStrip for browser at |to_browser_index|.
void MoveTab(int from_browser_index, int to_browser_index, int tab_index);

// Navigate the active tab for browser in position |index| to the given
// URL.
void NavigateTab(int browser_index, const GURL& url);

// Navigate the active tab for browser in position |index| back by one;
// if this isn't possible, does nothing
void NavigateTabBack(int browser_index);

// Navigate the active tab for browser in position |index| forward by
// one; if this isn't possible, does nothing
void NavigateTabForward(int browser_index);

// Wait for a session change to |web_contents| to propagate to the model
// associator. Will return true once |url| has been found, or false if it times
// out while waiting.
bool WaitForTabToLoad(int browser_index,
                      const GURL& url,
                      content::WebContents* web_contents);

// Wait for each url in |urls| to load. The ordering of |urls| is assumed to
// match the ordering of the corresponding tabs.
bool WaitForTabsToLoad(int browser_index, const std::vector<GURL>& urls);

// Stores a pointer to the local session for a given profile in |session|.
// Returns true on success, false on failure.
bool GetLocalSession(int browser_index,
                     const sync_sessions::SyncedSession** session);

// Deletes the foreign session with tag |session_tag| from the profile specified
// by |index|. This will affect all synced clients.
// Note: We pass the session_tag in by value to ensure it's not a reference
// to the session tag within the SyncedSession we plan to delete.
void DeleteForeignSession(int browser_index, std::string session_tag);

// Checker to block until the foreign sessions for a particular profile matches
// the local sessions from another profile.
class ForeignSessionsMatchChecker : public MultiClientStatusChangeChecker {
 public:
  ForeignSessionsMatchChecker(int profile_index, int foreign_profile_index);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const int profile_index_;
  const int foreign_profile_index_;
};

// Checker to block until the SESSIONS entities on the FakeServer match the
// given matcher.
class SessionEntitiesChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher = testing::Matcher<std::vector<sync_pb::SessionSpecifics>>;

  explicit SessionEntitiesChecker(const Matcher& matcher);
  ~SessionEntitiesChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

}  // namespace sessions_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SESSIONS_HELPER_H_
