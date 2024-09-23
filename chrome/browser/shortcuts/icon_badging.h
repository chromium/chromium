// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_ICON_BADGING_H_
#define CHROME_BROWSER_SHORTCUTS_ICON_BADGING_H_

#include <vector>

namespace gfx {
class ImageFamily;
}  // namespace gfx

class SkBitmap;

namespace shortcuts {

// Badges the Chrome logo, maintaining channel information in branded builds,
// and the chromium logo on non branded builds to the site icon. This will
// always return 9 images corresponding to commonly used shortcut sizes on
// current operating systems, all of which are available in `ShortcutSizes`.
gfx::ImageFamily ApplyProductLogoBadgeToIcons(std::vector<SkBitmap> icons);

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_ICON_BADGING_H_
