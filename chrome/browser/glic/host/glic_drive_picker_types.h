// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_DRIVE_PICKER_TYPES_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_DRIVE_PICKER_TYPES_H_

#include <cstdint>
#include <optional>
#include <string>

namespace glic {

// Reason why a Glic Drive picker request failed.
enum class GlicDrivePickerError {
  kUnknown,
  kWebSessionSignedOut,
  kChromeSignInRequired,
  kAccountMismatch,
  kAlreadyActive,
  kNoAnchorWindow,
  kLoadFailed,
  kDisabledByPolicy,
  kUnsupported,
};

// A sanitized Google Drive file selected by the user.
struct GlicDriveFileSelection {
  GlicDriveFileSelection();
  GlicDriveFileSelection(const GlicDriveFileSelection&);
  GlicDriveFileSelection& operator=(const GlicDriveFileSelection&);
  GlicDriveFileSelection(GlicDriveFileSelection&&);
  GlicDriveFileSelection& operator=(GlicDriveFileSelection&&);
  ~GlicDriveFileSelection();

  bool operator==(const GlicDriveFileSelection&) const = default;

  std::string id;
  std::string title;
  std::string mime_type;
  uint64_t size_bytes = 0;
  std::optional<std::string> resource_key;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_DRIVE_PICKER_TYPES_H_
