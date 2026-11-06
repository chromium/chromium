// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DESKTOP_MEDIA_ID_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DESKTOP_MEDIA_ID_H_

#include <optional>

#include "content/public/browser/desktop_media_id.h"
#include "ui/gfx/native_ui_types.h"

// Returns the DesktopMediaID of the browser window `window`, as the
// getDisplayMedia() picker creates it for Chrome's own windows, so that the
// video capture device launcher captures it with the viz-based window capture
// device. Returns std::nullopt if this is not possible.
std::optional<content::DesktopMediaID> GetDesktopMediaIDForBrowserWindow(
    gfx::NativeWindow window);

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DESKTOP_MEDIA_ID_H_
