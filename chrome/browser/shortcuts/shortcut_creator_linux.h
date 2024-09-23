// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_LINUX_H_
#define CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_LINUX_H_

#include <string>

#include "chrome/browser/shortcuts/shortcut_creator.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class Image;
}

namespace shortcuts {
class LinuxXdgWrapper;

// Output of the shortcut creation flow, consisting of the path where the
// shortcut was created and the creation result.
struct ShortcutCreatorOutput {
  base::FilePath shortcut_path;
  ShortcutCreatorResult result = ShortcutCreatorResult::kError;
};

// Creates a shortcut on the user's desktop and installs it into the system's
// desktop menu. The icon is written into the user's profile directory. This
// method makes blocking calls and must be called on an appropriate task runner.
// Invariants that will CHECK-fail:
// - The `shortcut_name` must not be empty.
// - The `shortcut_url` must be valid.
// - The `profile_path` must be be non-empty and the full path (not just the
//   base name).
//
// Side-effects from this method are encapsulated by:
// - LinuxXdgWrapper
// - The base::DIR_USER_DESKTOP path service variable.
// To test, use the ShortcutCreatorLinuxTestSupport to capture these
// side-effects.
ShortcutCreatorOutput CreateShortcutOnLinuxDesktop(
    const std::string& shortcut_name,
    const GURL& shortcut_url,
    const gfx::Image& icon,
    const base::FilePath& profile_path,
    LinuxXdgWrapper& xdg_wrapper);

// Overrides the LinuxXdgWrapper implementation used by the linux implementation
// of `CreateShortcutOnDesktop` (i.e. the LinuxXdgWrapper that gets passed by
// default to `CreateShortcutOnLinuxDesktop()`). Call with null to reset back to
// the default behavior.
// Consider using `ShortcutCreationTestSupport` rather than calling this method
// directly.
void SetDefaultXdgWrapperForTesting(LinuxXdgWrapper* xdg_wrapper);

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_LINUX_H_
