// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These "task_manager::browsertest_util" functions allow you to wait for a
// task manager to show a particular state, enabling tests of the form "do
// something that ought to create a process, then wait for that process to show
// up in the Task Manager." They are intended to abstract away the details of
// the platform's TaskManager UI.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_

#include <stddef.h>

#include "base/strings/string16.h"

namespace task_manager {
namespace browsertest_util {

// Specifies some integer-valued column of numeric data reported by the task
// manager model. Please add more here as needed by tests.
enum class ColumnSpecifier {
  PROCESS_ID,
  MEMORY_FOOTPRINT,
  V8_MEMORY,
  V8_MEMORY_USED,
  SQLITE_MEMORY_USED,
  IDLE_WAKEUPS,
  NETWORK_USE,
  TOTAL_NETWORK_USE,

  COLUMN_NONE,  // Default value.
};

// Runs the message loop, observing the task manager, until there are exactly
// |resource_count| many resources whose titles match the pattern
// |title_pattern|. The match is done via string_util's base::MatchPattern, so
// |title_pattern| may contain wildcards like "*".
//
// If the wait times out, this test will trigger a gtest failure. To get
// meaningful errors, tests should wrap invocations of this function with
// ASSERT_NO_FATAL_FAILURE().
void WaitForTaskManagerRows(int resource_count,
                            const base::string16& title_pattern);

// Make the indicated TaskManager column be visible.
void ShowColumn(ColumnSpecifier column_specifier);

// Waits for the row identified by |title_pattern| to be showing a numeric data
// value of at least |min_column_value| in the task manager column identified by
// |column_specifier|. As with WaitForTaskManagerRows(), |title_pattern| is
// meant to be a string returned by MatchTab() or similar.
//
// To get meaningful errors, tests should wrap invocations of this function with
// ASSERT_NO_FATAL_FAILURE().
void WaitForTaskManagerStatToExceed(const base::string16& title_pattern,
                                    ColumnSpecifier column_specifier,
                                    size_t min_column_value);

// ASCII matcher convenience functions for use with WaitForTaskManagerRows()
base::string16 MatchTab(const char* title);         // "Tab: " + title
base::string16 MatchAnyTab();                       // "Tab: *"
base::string16 MatchAboutBlankTab();                // "Tab: about:blank"
base::string16 MatchExtension(const char* title);   // "Extension: " + title
base::string16 MatchAnyExtension();                 // "Extension: *"
base::string16 MatchApp(const char* title);         // "App: " + title
base::string16 MatchAnyApp();                       // "App: *"
base::string16 MatchWebView(const char* title);     // "WebView: " + title
base::string16 MatchAnyWebView();                   // "WebView: *"
base::string16 MatchBackground(const char* title);  // "Background: " + title
base::string16 MatchAnyBackground();                // "Background: *"
base::string16 MatchPrint(const char* title);       // "Print: " + title
base::string16 MatchAnyPrint();                     // "Print: *"
base::string16 MatchSubframe(const char* title);    // "Subframe: " + title
base::string16 MatchAnySubframe();                  // "Subframe: *"
// "Utility: " + title
base::string16 MatchUtility(const base::string16& title);
base::string16 MatchAnyUtility();                   // "Utility: *"

}  // namespace browsertest_util
}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_
