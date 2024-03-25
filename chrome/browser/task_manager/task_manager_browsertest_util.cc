// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_browsertest_util.h"

#include <iomanip>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/task_manager/task_manager_tester.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"

namespace task_manager {
namespace browsertest_util {

namespace {

// Helper class to run a message loop until a TaskManagerTester is in an
// expected state. If timeout occurs, an ASCII version of the task manager's
// contents, along with a summary of the expected state, are dumped to test
// output, to assist debugging.
class ResourceChangeObserver {
 public:
  ResourceChangeObserver(size_t required_count,
                         const std::u16string& title_pattern,
                         ColumnSpecifier column_specifier,
                         size_t min_column_value)
      : required_count_(required_count),
        title_pattern_(title_pattern),
        column_specifier_(column_specifier),
        min_column_value_(min_column_value) {
    task_manager_tester_ = TaskManagerTester::Create(base::BindRepeating(
        &ResourceChangeObserver::OnResourceChange, base::Unretained(this)));
  }

  void RunUntilSatisfied() {
    // See if the condition is satisfied without having to run the loop. This
    // check has to be placed after the installation of the
    // TaskManagerModelObserver, because resources may change before that.
    if (IsSatisfied())
      return;

    timer_.Start(FROM_HERE, TestTimeouts::action_timeout(), this,
                 &ResourceChangeObserver::OnTimeout);

    run_loop_.Run();

    // If we succeeded normally (no timeout), check our post condition again
    // before returning control to the test. If it is no longer satisfied, the
    // test is likely flaky: we were waiting for a state that was only achieved
    // emphemerally), so treat this as a failure.
    if (!IsSatisfied() && timer_.IsRunning()) {
      FAIL() << "Wait condition satisfied only emphemerally. Likely test "
             << "problem. Maybe wait instead for the state below?\n"
             << DumpTaskManagerModel();
    }

    timer_.Stop();
  }

 private:
  void OnResourceChange() {
    if (!IsSatisfied())
      return;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop_.QuitClosure());
  }

  bool IsSatisfied() { return CountMatches() == required_count_; }

  size_t CountMatches() {
    size_t match_count = 0;
    for (size_t i = 0; i < task_manager_tester_->GetRowCount(); i++) {
      if (!base::MatchPattern(task_manager_tester_->GetRowTitle(i),
                              title_pattern_))
        continue;

      if (GetColumnValue(i) < min_column_value_)
        continue;

      match_count++;
    }
    return match_count;
  }

  int64_t GetColumnValue(int index) {
    return task_manager_tester_->GetColumnValue(column_specifier_, index);
  }

  const char* GetColumnName() {
    switch (column_specifier_) {
      case ColumnSpecifier::COLUMN_NONE:
        return "N/A";
      case ColumnSpecifier::PROCESS_ID:
        return "Process ID";
      case ColumnSpecifier::MEMORY_FOOTPRINT:
        return "Memory Footprint";
      case ColumnSpecifier::V8_MEMORY:
        return "V8 Memory";
      case ColumnSpecifier::V8_MEMORY_USED:
        return "V8 Memory Used";
      case ColumnSpecifier::SQLITE_MEMORY_USED:
        return "SQLite Memory Used";
      case ColumnSpecifier::IDLE_WAKEUPS:
        return "Idle wake ups";
      case ColumnSpecifier::NETWORK_USE:
        return "Network";
      case ColumnSpecifier::TOTAL_NETWORK_USE:
        return "Total Network";
    }
    return "N/A";
  }

  void OnTimeout() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop_.QuitClosure());
    FAIL() << "Timed out.\n" << DumpTaskManagerModel();
  }

  testing::Message DumpTaskManagerModel() {
    testing::Message task_manager_state_dump;
    task_manager_state_dump << "Waiting for exactly " << required_count_
                            << " matches of wildcard pattern \""
                            << base::UTF16ToASCII(title_pattern_) << "\"";
    if (min_column_value_ > 0) {
      task_manager_state_dump << " && [" << GetColumnName()
                              << " >= " << min_column_value_ << "]";
    }
    task_manager_state_dump << "\nCurrently there are " << CountMatches()
                            << " matches.";
    task_manager_state_dump << "\nCurrent Task Manager Model is:";
    for (size_t i = 0; i < task_manager_tester_->GetRowCount(); i++) {
      task_manager_state_dump
          << "\n  > " << std::setw(40) << std::left
          << base::UTF16ToASCII(task_manager_tester_->GetRowTitle(i));
      if (min_column_value_ > 0) {
        task_manager_state_dump << " [" << GetColumnName()
                                << " == " << GetColumnValue(i) << "]";
      }
    }
    return task_manager_state_dump;
  }

  std::unique_ptr<TaskManagerTester> task_manager_tester_;
  const size_t required_count_;
  const std::u16string title_pattern_;
  const ColumnSpecifier column_specifier_;
  const int64_t min_column_value_;
  base::RunLoop run_loop_;
  base::OneShotTimer timer_;
};

}  // namespace

void WaitForTaskManagerRows(size_t required_count,
                            const std::u16string& title_pattern) {
  constexpr size_t kColumnValueDontCare = 0;
  ResourceChangeObserver observer(required_count, title_pattern,
                                  ColumnSpecifier::COLUMN_NONE,
                                  kColumnValueDontCare);
  observer.RunUntilSatisfied();
}

void WaitForTaskManagerStatToExceed(const std::u16string& title_pattern,
                                    ColumnSpecifier column_getter,
                                    size_t min_column_value) {
  constexpr size_t kWaitForOneMatch = 1;
  ResourceChangeObserver observer(kWaitForOneMatch, title_pattern,
                                  column_getter, min_column_value);
  observer.RunUntilSatisfied();
}

std::u16string MatchTab(std::string_view title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX,
                                    base::UTF8ToUTF16(title));
}

std::u16string MatchAnyTab() {
  return MatchTab("*");
}

std::u16string MatchAboutBlankTab() {
  return MatchTab("about:blank");
}

std::u16string MatchIncognitoTab(std::string_view title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_INCOGNITO_PREFIX,
                                    base::UTF8ToUTF16(title));
}

std::u16string MatchAnyIncognitoTab() {
  return MatchIncognitoTab("*");
}

std::u16string MatchExtension(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_EXTENSION_PREFIX,
                                    base::ASCIIToUTF16(title));
}

std::u16string MatchAnyExtension() {
  return MatchExtension("*");
}

std::u16string MatchApp(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_APP_PREFIX,
                                    base::ASCIIToUTF16(title));
}

std::u16string MatchAnyApp() {
  return MatchApp("*");
}

std::u16string MatchWebView(const char* title) {
  return l10n_util::GetStringFUTF16(
      IDS_EXTENSION_TASK_MANAGER_WEBVIEW_TAG_PREFIX, base::ASCIIToUTF16(title));
}

std::u16string MatchAnyWebView() {
  return MatchWebView("*");
}

std::u16string MatchBackground(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_BACKGROUND_PREFIX,
                                    base::ASCIIToUTF16(title));
}

std::u16string MatchAnyBackground() {
  return MatchBackground("*");
}

std::u16string MatchPrint(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRINT_PREFIX,
                                    base::ASCIIToUTF16(title));
}

std::u16string MatchAnyPrint() {
  return MatchPrint("*");
}

std::u16string MatchSubframe(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_SUBFRAME_PREFIX,
                                    base::ASCIIToUTF16(title));
}

std::u16string MatchAnySubframe() {
  return MatchSubframe("*");
}

std::u16string MatchUtility(const std::u16string& title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX, title);
}

std::u16string MatchAnyUtility() {
  return MatchUtility(u"*");
}

std::u16string MatchBFCache(std::string_view title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_BACK_FORWARD_CACHE_PREFIX,
                                    base::UTF8ToUTF16(title));
}

std::u16string MatchAnyBFCache() {
  return MatchBFCache("*");
}

std::u16string MatchPrerender(std::string_view title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRERENDER_PREFIX,
                                    base::UTF8ToUTF16(title));
}

std::u16string MatchAnyPrerender() {
  return MatchPrerender("*");
}

std::u16string MatchFencedFrame(std::string_view title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_FENCED_FRAME_PREFIX,
                                    base::UTF8ToUTF16(title));
}

std::u16string MatchAnyFencedFrame() {
  return MatchFencedFrame("*");
}

std::u16string MatchIncognitoFencedFrame(std::string_view title) {
  return l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_FENCED_FRAME_INCOGNITO_PREFIX, base::UTF8ToUTF16(title));
}

std::u16string MatchAnyIncognitoFencedFrame() {
  return MatchIncognitoFencedFrame("*");
}
}  // namespace browsertest_util
}  // namespace task_manager
