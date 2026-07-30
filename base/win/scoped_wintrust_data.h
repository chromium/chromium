// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_WINTRUST_DATA_H_
#define BASE_WIN_SCOPED_WINTRUST_DATA_H_

#include <memory>

#include "base/base_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/win/windows_types.h"

namespace base::win {

// RAII wrapper that initializes WINTRUST_FILE_INFO and WINTRUST_DATA
// structures, executes WinVerifyTrust on `binary_path`, and automatically
// releases state data handles upon destruction.
// Exposed in the public interface so callers that need to inspect the trust
// state data (e.g., extracting certificate chains via `hWVTStateData()`) can do
// so and pass the open state to `IsWintrustDataTrusted()` without calling
// WinVerifyTrust multiple times.
class BASE_EXPORT ScopedWintrustData {
 public:
  explicit ScopedWintrustData(const FilePath& binary_path);
  ScopedWintrustData(const FilePath& binary_path, HANDLE file_handle);
  ~ScopedWintrustData();

  ScopedWintrustData(const ScopedWintrustData&) = delete;
  ScopedWintrustData& operator=(const ScopedWintrustData&) = delete;

  // Returns the return code from `WinVerifyTrust` (e.g., `ERROR_SUCCESS`,
  // `TRUST_E_NOSIGNATURE`, `TRUST_E_SUBJECT_NOT_TRUSTED`).
  LONG status() const;

  // Returns true if `status()` is `ERROR_SUCCESS`, indicating that the
  // binary's Authenticode signature was successfully verified.
  bool is_valid() const;

  // Returns the state data handle (`hWVTStateData`) from the underlying
  // `WINTRUST_DATA` structure, or `nullptr` if unavailable. Can be passed to
  // functions like `WTHelperProvDataFromStateData` while this object is in
  // scope.
  HANDLE hWVTStateData() const;

 private:
  void Initialize(const FilePath& binary_path, HANDLE file_handle);

  base::File file_;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace base::win

#endif  // BASE_WIN_SCOPED_WINTRUST_DATA_H_

