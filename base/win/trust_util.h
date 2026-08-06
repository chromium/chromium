// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_TRUST_UTIL_H_
#define BASE_WIN_TRUST_UTIL_H_

#include "base/base_export.h"
#include "base/files/file_path.h"
#include "base/win/scoped_wintrust_data.h"
#include "base/win/windows_types.h"

namespace base::win {

// Verifies if an existing `wintrust_data` represents a valid trusted
// signature, optionally checking the publisher name. This avoids redundant
// WinVerifyTrust calls when callers already hold a `ScopedWintrustData`
// instance to inspect certificate chains or state data.
BASE_EXPORT bool IsWintrustDataTrusted(const ScopedWintrustData& wintrust_data,
                                       bool verify_publisher = true);

// Verifies the Authenticode signature of the binary at `binary_path`.
// - If `verify_publisher` is true, verifies that the certificate subject
//   matches the running process executable's subject.
// - In non-release branded builds (i.e., without GOOGLE_CHROME_BRANDING and
//   NDEBUG), returns `true` by default unless `force_verify_in_dev_builds` is
//   true.
BASE_EXPORT bool IsBinaryTrusted(const FilePath& binary_path,
                                 bool verify_publisher = true,
                                 bool force_verify_in_dev_builds = false);

}  // namespace base::win

#endif  // BASE_WIN_TRUST_UTIL_H_
