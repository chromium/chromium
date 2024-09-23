// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATION_TEST_SUPPORT_H_
#define CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATION_TEST_SUPPORT_H_

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_path_override.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;
namespace base {
class FilePath;
}

namespace shortcuts {

// Helper class that overrides what needs to be overridden to make sure that
// tests creating shortcuts don't modify state of the operating system. This
// sets up an override for the "desktop" directory, as well as platform specific
// overrides where needed. Also contains some helper methods for matchers to
// verify shortcuts were created correctly.
class ShortcutCreationTestSupport {
 public:
  ShortcutCreationTestSupport();
  ~ShortcutCreationTestSupport();

  // Launches the shortcut identified by `path` in the current chrome (or
  // browser test) instance, as if it was launched by the user or operating
  // system.
  static void LaunchShortcut(const base::FilePath& path);

  // These methods are implementation details for the matchers below. Don't call
  // these methods directly and instead use the matchers.
  static bool ShortcutIsForUrl(const base::FilePath& path,
                               const GURL& url,
                               ::testing::MatchResultListener* result_listener);
  static bool ShortcutIsForProfile(
      const base::FilePath& path,
      const base::FilePath& profile_path,
      ::testing::MatchResultListener* result_listener);
  static bool ShortcutHasTitle(const base::FilePath& path,
                               const std::u16string& title,
                               ::testing::MatchResultListener* result_listener);

 private:
  base::ScopedPathOverride desktop_override_{base::DIR_USER_DESKTOP};
  base::ScopedClosureRunner cleanup_test_overrides_;
};

// Matcher that verifies that `argument` is a shortcut given by a
// `base::FilePath` that links to the given `url`.
MATCHER_P(IsShortcutForUrl, url, "") {
  return ShortcutCreationTestSupport::ShortcutIsForUrl(arg, url,
                                                       result_listener);
}

// Matcher that verifies that `argument` is a shortcut given by a
// `base::FilePath` that is configured to open in the given `profile_path`. The
// expected `profile_path` can either be a full profile path or just its base
// name (and can be a file path literal or an actual base::FilePath)). Only the
// base name is used for comparisons.
MATCHER_P(IsShortcutForProfile, profile_path, "") {
  return ShortcutCreationTestSupport::ShortcutIsForProfile(
      arg, base::FilePath(profile_path), result_listener);
}

// Matcher that verifies that `argument` is a shortcut given by a
// `base::FilePath` that has the given `title`. Note that on some platforms
// lossy transformations are done on the title as serialized to the shortcut
// file. As such one file might match multiple titles.
MATCHER_P(IsShortcutWithTitle, title, "") {
  return ShortcutCreationTestSupport::ShortcutHasTitle(arg, title,
                                                       result_listener);
}

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATION_TEST_SUPPORT_H_
