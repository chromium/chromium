// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_SAFEARRAY_H_
#define BASE_WIN_SCOPED_SAFEARRAY_H_

#include <objbase.h>

#include "base/base_export.h"
#include "base/logging.h"

namespace base {
namespace win {

// Manages a Windows SAFEARRAY. This is a minimal wrapper that simply provides
// RAII semantics and does not duplicate the extensive functionality that
// CComSafeArray offers.
class BASE_EXPORT ScopedSafearray {
 public:
  explicit ScopedSafearray(SAFEARRAY* safearray = nullptr)
      : safearray_(safearray) {}

  // Move constructor
  ScopedSafearray(ScopedSafearray&& r) noexcept : safearray_(r.safearray_) {
    r.safearray_ = nullptr;
  }

  // Move operator=. Allows assignment from a ScopedSafearray rvalue.
  ScopedSafearray& operator=(ScopedSafearray&& rvalue) {
    Reset(rvalue.Release());
    return *this;
  }

  ~ScopedSafearray() { Destroy(); }

  void Destroy() {
    if (safearray_) {
      HRESULT hr = SafeArrayDestroy(safearray_);
      DCHECK_EQ(S_OK, hr);
      safearray_ = nullptr;
    }
  }

  // Give ScopedSafearray ownership over an already allocated SAFEARRAY or
  // nullptr.
  void Reset(SAFEARRAY* safearray = nullptr) {
    if (safearray != safearray_) {
      Destroy();
      safearray_ = safearray;
    }
  }

  // Releases ownership of the SAFEARRAY to the caller.
  SAFEARRAY* Release() {
    SAFEARRAY* safearray = safearray_;
    safearray_ = nullptr;
    return safearray;
  }

  // Retrieves the pointer address.
  // Used to receive SAFEARRAYs as out arguments (and take ownership).
  // This function releases any existing references because it will leak
  // the existing ref otherwise.
  // Usage: GetSafearray(safearray.Receive());
  SAFEARRAY** Receive() {
    Destroy();
    return &safearray_;
  }

  // Returns the internal pointer. Prefer using operator SAFEARRAY*() instead,
  // as that will automatically convert for function calls expecting a raw
  // SAFEARRAY*
  SAFEARRAY* Get() const { return safearray_; }

  // Forbid comparison of ScopedSafearray types.  You should never have the same
  // SAFEARRAY owned by two different scoped_ptrs.
  bool operator==(const ScopedSafearray& safearray2) const = delete;
  bool operator!=(const ScopedSafearray& safearray2) const = delete;

 private:
  SAFEARRAY* safearray_;
  DISALLOW_COPY_AND_ASSIGN(ScopedSafearray);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SCOPED_SAFEARRAY_H_
