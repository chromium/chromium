// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_UTILS_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_UTILS_H_

#include "base/memory/weak_ptr.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace extensions {
class StreamContainer;
}  // namespace extensions

namespace save_to_drive {

// Returns the `StreamContainer` weak ptr associated with the given
// `render_frame_host`.
base::WeakPtr<extensions::StreamContainer> GetStreamWeakPtr(
    content::RenderFrameHost* render_frame_host);

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_UTILS_H_
