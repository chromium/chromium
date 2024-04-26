// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_H_
#define CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_H_

#include <string>

#include "base/functional/callback_forward.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class ImageFamily;
}

namespace shortcuts {

enum class ShortcutCreatorResult { kSuccess, kError };

using ShortcutCreatorCallback = base::OnceCallback<void(ShortcutCreatorResult)>;

// Creates a shortcut on the OS desktop with the given name and icons. When
// clicked / launched, it will launch the given url in a new chrome tab.
// Invariants:
// - The url must be valid.
// - The shortcut name must not be empty.
// - The icon images must not be empty.
// - The profile path must be valid / non-empty, and the full profile path
//   (not just the base name).
// - This must be called on a sequence that allows blocking calls.
//
// `complete` will be called on the same sequence as
// `CreateShortcutOnUserDesktop` was called on. Use BindPostTask or similar if
// you want the callback to be called on a different sequence.
void CreateShortcutOnUserDesktop(const std::string& shortcut_name,
                                 const GURL& shortcut_url,
                                 gfx::ImageFamily icon_images,
                                 const base::FilePath& profile_path,
                                 ShortcutCreatorCallback complete);

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_H_
