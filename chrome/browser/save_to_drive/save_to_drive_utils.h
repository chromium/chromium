// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_UTILS_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_UTILS_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace extensions {
class StreamContainer;
}  // namespace extensions

namespace save_to_drive {

// The number of trailing bytes in which the `%%EOF` marker is expected to
// appear.
inline constexpr size_t kPdfEofSearchWindow = 1024;

// Returns the sanitized title with `.pdf` extension.
// 1. Strips away the existing extension (if any).
// 2. Appends the `.pdf` extension.
std::u16string EnsurePdfExtension(const std::u16string& title);

// Returns the `StreamContainer` weak ptr associated with the given
// `render_frame_host`.
base::WeakPtr<extensions::StreamContainer> GetStreamWeakPtr(
    content::RenderFrameHost* render_frame_host);

// Gets the tab id associated with the given `render_frame_host`.
int GetTabId(content::RenderFrameHost* render_frame_host);

// Validates that the buffer starts with the PDF magic bytes "%PDF-".
bool ValidatePdfMagic(base::span<const uint8_t> buffer);

// Validates that the buffer contains the PDF end-of-file marker "%%EOF" within
// the last `kPdfEofSearchWindow` bytes.
bool ValidatePdfEof(base::span<const uint8_t> buffer);

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_UTILS_H_
