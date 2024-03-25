// Copyright 2016 The Chromium Authors
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

#include <string>
#include <string_view>

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
void WaitForTaskManagerRows(size_t resource_count,
                            const std::u16string& title_pattern);

// Make the indicated TaskManager column be visible.
void ShowColumn(ColumnSpecifier column_specifier);

// Waits for the row identified by |title_pattern| to be showing a numeric data
// value of at least |min_column_value| in the task manager column identified by
// |column_specifier|. As with WaitForTaskManagerRows(), |title_pattern| is
// meant to be a string returned by MatchTab() or similar.
//
// To get meaningful errors, tests should wrap invocations of this function with
// ASSERT_NO_FATAL_FAILURE().
void WaitForTaskManagerStatToExceed(const std::u16string& title_pattern,
                                    ColumnSpecifier column_specifier,
                                    size_t min_column_value);

// ASCII matcher convenience functions for use with WaitForTaskManagerRows()
std::u16string MatchTab(std::string_view title);    // "Tab: " + title
std::u16string MatchAnyTab();                       // "Tab: *"
std::u16string MatchAboutBlankTab();                // "Tab: about:blank"
std::u16string MatchIncognitoTab(std::string_view title);
std::u16string MatchAnyIncognitoTab();
std::u16string MatchExtension(const char* title);   // "Extension: " + title
std::u16string MatchAnyExtension();                 // "Extension: *"
std::u16string MatchApp(const char* title);         // "App: " + title
std::u16string MatchAnyApp();                       // "App: *"
std::u16string MatchWebView(const char* title);     // "WebView: " + title
std::u16string MatchAnyWebView();                   // "WebView: *"
std::u16string MatchBackground(const char* title);  // "Background: " + title
std::u16string MatchAnyBackground();                // "Background: *"
std::u16string MatchPrint(const char* title);       // "Print: " + title
std::u16string MatchAnyPrint();                     // "Print: *"
std::u16string MatchSubframe(const char* title);    // "Subframe: " + title
std::u16string MatchAnySubframe();                  // "Subframe: *"
// "Utility: " + title
std::u16string MatchUtility(const std::u16string& title);
std::u16string MatchAnyUtility();  // "Utility: *"
std::u16string MatchBFCache(std::string_view title);
std::u16string MatchAnyBFCache();
std::u16string MatchPrerender(std::string_view title);
std::u16string MatchAnyPrerender();
std::u16string MatchFencedFrame(std::string_view title);
std::u16string MatchAnyFencedFrame();
std::u16string MatchIncognitoFencedFrame(std::string_view title);
std::u16string MatchAnyIncognitoFencedFrame();
}  // namespace browsertest_util
}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_
