// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_log.h"

#include <stddef.h>

#include <list>
#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/session_restore_policy.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_restore_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/page_transition_types.h"

class SessionServiceLogTest : public InProcessBrowserTest {
 public:
  SessionServiceLogTest() = default;
  ~SessionServiceLogTest() override = default;

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(nkostylev): Investigate if we can remove this switch.
    command_line->AppendSwitch(switches::kCreateBrowserOnStartupForTests);
  }
#endif

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    ASSERT_TRUE(browser());
    profile_ = browser()->profile();
    ASSERT_TRUE(profile_);
  }

  void SetUpOnMainThread() override {
    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(browser()->profile(), pref);
  }

  std::optional<SessionServiceEvent> FindMostRecentEventOfType(
      SessionServiceEventLogType type) const {
    auto events = GetSessionServiceEvents(profile_);
    for (const SessionServiceEvent& event : base::Reversed(events)) {
      if (event.type == type)
        return event;
    }
    return std::nullopt;
  }

  std::list<SessionServiceEvent>::reverse_iterator
  AdvanceToMostRecentEventOfType(
      const std::list<SessionServiceEvent>& events,
      std::list<SessionServiceEvent>::reverse_iterator start,
      SessionServiceEventLogType type) {
    while (start != events.rend() && start->type != type)
      ++start;
    return start;
  }

  int GetEventCountOfType(SessionServiceEventLogType type) const {
    int count = 0;
    for (const auto& event : GetSessionServiceEvents(profile_)) {
      if (event.type == type)
        ++count;
    }
    return count;
  }

 protected:
  // Cached as browser() may be destroyed.
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_ = nullptr;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
};

IN_PROC_BROWSER_TEST_F(SessionServiceLogTest, ExitEvent) {
  // A start event should always be logged.
  EXPECT_TRUE(FindMostRecentEventOfType(SessionServiceEventLogType::kStart));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_FALSE(FindMostRecentEventOfType(SessionServiceEventLogType::kExit));
  const int tab_count = browser()->tab_strip_model()->count();
  CloseBrowserSynchronously(browser());
  auto exit_event =
      FindMostRecentEventOfType(SessionServiceEventLogType::kExit);
  ASSERT_TRUE(exit_event);
  EXPECT_EQ(1, exit_event->data.exit.window_count);
  EXPECT_EQ(tab_count, exit_event->data.exit.tab_count);
}

IN_PROC_BROWSER_TEST_F(SessionServiceLogTest, PRE_RestoreEvent) {
  // A start event should always be logged.
  EXPECT_TRUE(FindMostRecentEventOfType(SessionServiceEventLogType::kStart));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  Browser* new_browser = chrome::OpenEmptyWindow(profile_);
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());
  ui_test_utils::NavigateToURLWithDisposition(
      new_browser, GURL(url::kAboutBlankURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

IN_PROC_BROWSER_TEST_F(SessionServiceLogTest, RestoreEvent) {
  auto events = GetSessionServiceEvents(profile_);

  // There should be a restore.
  auto iter = events.rbegin();
  iter = AdvanceToMostRecentEventOfType(events, iter,
                                        SessionServiceEventLogType::kRestore);
  ASSERT_TRUE(iter != events.rend());
  EXPECT_EQ(2, iter->data.restore.window_count);
  EXPECT_EQ(4, iter->data.restore.tab_count);

  // Preceded by a start (for this test).
  iter = AdvanceToMostRecentEventOfType(events, iter,
                                        SessionServiceEventLogType::kStart);
  ASSERT_TRUE(iter != events.rend());

  // Exit.
  iter = AdvanceToMostRecentEventOfType(events, iter,
                                        SessionServiceEventLogType::kExit);
  ASSERT_TRUE(iter != events.rend());
  EXPECT_EQ(2, iter->data.exit.window_count);
  EXPECT_EQ(4, iter->data.exit.tab_count);

  // And another start from PRE_ test.
  iter = AdvanceToMostRecentEventOfType(events, iter,
                                        SessionServiceEventLogType::kStart);
  ASSERT_TRUE(iter != events.rend());
}
