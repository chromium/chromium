// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"

#include <AppKit/AppKit.h>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/shortcuts/chrome_webloc_file.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace shortcuts {

ShortcutCreationTestSupport::ShortcutCreationTestSupport() = default;
ShortcutCreationTestSupport::~ShortcutCreationTestSupport() = default;

// static
void ShortcutCreationTestSupport::LaunchShortcut(const base::FilePath& path) {
  [NSApp.delegate application:NSApp
                     openURLs:@[ base::apple::FilePathToNSURL(path) ]];
}

// static
bool ShortcutCreationTestSupport::ShortcutIsForUrl(
    const base::FilePath& path,
    const GURL& url,
    testing::MatchResultListener* result_listener) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::optional<ChromeWeblocFile> webloc = ChromeWeblocFile::LoadFromFile(path);
  if (!webloc.has_value()) {
    *result_listener << path << " is not a valid .crwebloc file";
    return false;
  }

  *result_listener << "Target URL in .crwebloc file: " << webloc->target_url();
  return webloc->target_url() == url;
}

// static
bool ShortcutCreationTestSupport::ShortcutIsForProfile(
    const base::FilePath& path,
    const base::FilePath& profile_path,
    testing::MatchResultListener* result_listener) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::optional<ChromeWeblocFile> webloc = ChromeWeblocFile::LoadFromFile(path);
  if (!webloc.has_value()) {
    *result_listener << path << " is not a valid .crwebloc file";
    return false;
  }

  *result_listener << "Profile in .crwebloc file: "
                   << webloc->profile_path_name().path();
  return webloc->profile_path_name().path() == profile_path.BaseName();
}

// static
bool ShortcutCreationTestSupport::ShortcutHasTitle(
    const base::FilePath& path,
    const std::u16string& title,
    ::testing::MatchResultListener* result_listener) {
  std::string safe_title = base::UTF16ToUTF8(title);
  base::ReplaceChars(safe_title, "/", ":", &safe_title);
  return ExplainMatchResult(::testing::StartsWith(safe_title),
                            path.BaseName().value(), result_listener);
}

}  // namespace shortcuts
