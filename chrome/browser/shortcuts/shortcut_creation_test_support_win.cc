// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"

#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/shortcut.h"
#include "chrome/browser/chrome_browser_main.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace shortcuts {

namespace {
std::optional<base::CommandLine> CommandLineFromShortcut(
    const base::FilePath& shortcut_path) {
  base::FilePath target_path;
  std::wstring args;
  if (!base::win::ResolveShortcut(shortcut_path, &target_path, &args)) {
    return std::nullopt;
  }
  // base::CommandLine expects the command line to start with the executable
  // name. While we could prepend `target_path`, we'd then have to make sure it
  // is properly escaped. Since we don't actually care about the executable in
  // the command line, prepend an arbitrary executable name instead.
  return base::CommandLine::FromString(L"chrome.exe " + args);
}
}  // namespace

ShortcutCreationTestSupport::ShortcutCreationTestSupport() = default;
ShortcutCreationTestSupport::~ShortcutCreationTestSupport() = default;

// static
void ShortcutCreationTestSupport::LaunchShortcut(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_io;
  auto command_line = CommandLineFromShortcut(path);
  CHECK(command_line.has_value());
  ChromeBrowserMainParts::ProcessSingletonNotificationCallback(
      *command_line, /*current_directory=*/{});
}

// static
bool ShortcutCreationTestSupport::ShortcutIsForUrl(
    const base::FilePath& path,
    const GURL& url,
    testing::MatchResultListener* result_listener) {
  base::ScopedAllowBlockingForTesting allow_io;
  auto command_line = CommandLineFromShortcut(path);
  if (!command_line.has_value()) {
    *result_listener << "Couldn't resolve shortcut at " << path;
    return false;
  }
  *result_listener << "Target: " << command_line->GetCommandLineString();
  std::vector<std::wstring> arguments = command_line->GetArgs();
  return arguments.size() == 1 && arguments[0] == base::ASCIIToWide(url.spec());
}

// static
bool ShortcutCreationTestSupport::ShortcutIsForProfile(
    const base::FilePath& path,
    const base::FilePath& profile_path,
    testing::MatchResultListener* result_listener) {
  base::ScopedAllowBlockingForTesting allow_io;
  auto command_line = CommandLineFromShortcut(path);
  if (!command_line.has_value()) {
    *result_listener << "Couldn't resolve shortcut at " << path;
    return false;
  }
  *result_listener << "Target: " << command_line->GetCommandLineString();
  return command_line->GetSwitchValuePath("profile-directory") ==
         profile_path.BaseName();
}

// static
bool ShortcutCreationTestSupport::ShortcutHasTitle(
    const base::FilePath& path,
    const std::u16string& title,
    ::testing::MatchResultListener* result_listener) {
  return ExplainMatchResult(::testing::StartsWith(base::UTF16ToWide(title)),
                            path.BaseName().value(), result_listener);
}

}  // namespace shortcuts
