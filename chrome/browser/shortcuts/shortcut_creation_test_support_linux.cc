// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/shortcuts/fake_linux_xdg_wrapper.h"
#include "chrome/browser/shortcuts/linux_xdg_wrapper.h"
#include "chrome/browser/shortcuts/shortcut_creation_test_support_linux.h"
#include "chrome/browser/shortcuts/shortcut_creator_linux.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace shortcuts {

namespace {
std::string GetDesktopEntry(const std::string& key,
                            const std::string& desktop_file_contents) {
  return shell_integration_linux::internal::
      GetDesktopEntryStringValueFromFromDesktopFileForTest(
          key, desktop_file_contents);
}
}  // namespace

namespace internal {

std::vector<std::string> ParseDesktopExecForCommandLine(const std::string& s) {
  std::vector<std::string> result;
  std::string current_arg;
  bool in_quoted_arg = false;
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (in_quoted_arg) {
      // We're inside a "quoted argument" (i.e. have encountered a opening `"`
      // but not yet a closing `"`).
      if (c == '\\') {
        // `\` is an escape character for the next character, so skip the '\'
        // and append the next character to the current argument.
        CHECK_LT(i + 1, s.length());
        current_arg += s[++i];
      } else if (c == '"') {
        // `"` (which was not escaped) indicates the end of the quoted argument.
        // Push the argument to `result` and reset state ready for the next
        // argument.
        result.push_back(std::move(current_arg));
        in_quoted_arg = false;
        current_arg = "";
      } else {
        current_arg += c;
      }
    } else if (c == '"') {
      // `"` signals the start of a quoted argument. This should only occur when
      // we're not already in the middle of an argument.
      CHECK(current_arg.empty());
      in_quoted_arg = true;
    } else if (c == ' ' || c == '\t' || c == '\n') {
      // Whitespace deliminates arguments, so if the current argument is
      // non-empty, push it to `result`. The non-empty check ensures any
      // sequence of whitespace is treated as a single argument deliminator.
      if (!current_arg.empty()) {
        result.push_back(current_arg);
        current_arg = "";
      }
    } else {
      current_arg += c;
    }
  }
  CHECK(!in_quoted_arg);
  if (!current_arg.empty()) {
    result.push_back(current_arg);
  }
  return result;
}

}  // namespace internal

ShortcutCreationTestSupport::ShortcutCreationTestSupport() {
  std::unique_ptr<LinuxXdgWrapper> xdg_wrapper =
      std::make_unique<FakeLinuxXdgWrapper>();
  SetDefaultXdgWrapperForTesting(xdg_wrapper.get());
  cleanup_test_overrides_ = base::ScopedClosureRunner(base::BindOnce(
      [](std::unique_ptr<LinuxXdgWrapper>) {
        SetDefaultXdgWrapperForTesting(nullptr);
      },
      std::move(xdg_wrapper)));
}

ShortcutCreationTestSupport::~ShortcutCreationTestSupport() = default;

// static
void ShortcutCreationTestSupport::LaunchShortcut(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::string file;
  CHECK(base::ReadFileToString(path, &file));
  base::CommandLine command_line(
      internal::ParseDesktopExecForCommandLine(GetDesktopEntry("Exec", file)));
  ChromeBrowserMainParts::ProcessSingletonNotificationCallback(
      command_line, /*current_directory=*/{});
}

// static
bool ShortcutCreationTestSupport::ShortcutIsForUrl(
    const base::FilePath& path,
    const GURL& url,
    testing::MatchResultListener* result_listener) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::string file;
  if (!base::ReadFileToString(path, &file)) {
    *result_listener << "Could not load .desktop file from " << path;
    return false;
  }

  const std::string exec_value = GetDesktopEntry("Exec", file);
  const std::string url_value = GetDesktopEntry("URL", file);
  *result_listener << "Exec: " << exec_value << ", URL: " << url_value;
  return url_value == url.spec() &&
         exec_value.find(url.spec()) != std::string::npos;
}

// static
bool ShortcutCreationTestSupport::ShortcutIsForProfile(
    const base::FilePath& path,
    const base::FilePath& profile_path,
    testing::MatchResultListener* result_listener) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::string file;
  if (!base::ReadFileToString(path, &file)) {
    *result_listener << "Could not load .desktop file from " << path;
    return false;
  }

  const std::string exec_value = GetDesktopEntry("Exec", file);
  *result_listener << "Exec: " << exec_value;
  return exec_value.find(base::StringPrintf(
             "--profile-directory=%s",
             profile_path.BaseName().value().c_str())) != std::string::npos;
}

// static
bool ShortcutCreationTestSupport::ShortcutHasTitle(
    const base::FilePath& path,
    const std::u16string& title,
    ::testing::MatchResultListener* result_listener) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::string file;
  if (!base::ReadFileToString(path, &file)) {
    *result_listener << "Could not load .desktop file from " << path;
    return false;
  }

  const std::string name_value = GetDesktopEntry("Name", file);

  std::string safe_title = base::UTF16ToUTF8(title);
  base::ReplaceChars(safe_title, " \n\r", "_", &safe_title);
  // The actual linux shortcut code has more logic around illegal characters in
  // file names that might need to be stripped. For now tests using this matcher
  // don't use such file names, so the simpler approach here is good enough. If
  // we do need to use this matcher in tests for such edge cases, the logic here
  // might need to be updated.
  return ExplainMatchResult(::testing::HasSubstr(safe_title),
                            path.BaseName().value(), result_listener) &&
         ExplainMatchResult(::testing::Eq(base::UTF16ToUTF8(title)), name_value,
                            result_listener);
}

}  // namespace shortcuts
