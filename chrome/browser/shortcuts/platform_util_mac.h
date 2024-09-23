// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_PLATFORM_UTIL_MAC_H_
#define CHROME_BROWSER_SHORTCUTS_PLATFORM_UTIL_MAC_H_

#include <optional>

#include "base/files/safe_base_name.h"
#include "base/functional/callback_forward.h"

@class NSError;
@class NSImage;
@class NSURL;

namespace base {
class FilePath;
}

namespace shortcuts {

// Wrapper around NSWorkspace setIcon:forFile: that can be called from any
// thread. While the underlying NSWorkspace method can also be called from any
// thread, it is not reentrancy safe (i.e. multiple calls to it need to be
// sequenced). This wrapper takes care of the sequencing needed. The `callback`
// will be called on the calling sequence.
void SetIconForFile(NSImage* image,
                    const base::FilePath& file,
                    base::OnceCallback<void(bool)> callback);

// Wrapper around NSWorkspace setDefaultApplicationAtURL:toOpenFileAtURL:. The
// NSWorkspace method only exists on macOS 12.0 and newer; this method polyfills
// its implementation for older macOS versions.
void SetDefaultApplicationToOpenFile(
    NSURL* file_url,
    NSURL* application_url,
    base::OnceCallback<void(NSError*)> callback);

// Return a version of `title` that is safe to use as a filename on macOS.
std::optional<base::SafeBaseName> SanitizeTitleForFileName(
    const std::string& title);

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_PLATFORM_UTIL_MAC_H_
