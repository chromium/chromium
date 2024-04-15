// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/com_init_balancer.h"

#include <objbase.h>

#include "base/check_op.h"

namespace base {
namespace win {
namespace internal {

ComInitBalancer::ComInitBalancer(DWORD co_init) : co_init_(co_init) {
  ULARGE_INTEGER spy_cookie = {};
  HRESULT hr = ::CoRegisterInitializeSpy(this, &spy_cookie);
  if (SUCCEEDED(hr))
    spy_cookie_ = spy_cookie;
}

ComInitBalancer::~ComInitBalancer() {
  DCHECK(!spy_cookie_.has_value());
}

void ComInitBalancer::Disable() {
  if (spy_cookie_.has_value()) {
    ::CoRevokeInitializeSpy(spy_cookie_.value());
    reference_count_ = 0;
    spy_cookie_.reset();
  }
}

DWORD ComInitBalancer::GetReferenceCountForTesting() const {
  return reference_count_;
}

IFACEMETHODIMP
ComInitBalancer::PreInitialize(DWORD apartment_type, DWORD reference_count) {
  return S_OK;
}

IFACEMETHODIMP
ComInitBalancer::PostInitialize(HRESULT result,
                                DWORD apartment_type,
                                DWORD new_reference_count) {
  reference_count_ = new_reference_count;
  return result;
}

IFACEMETHODIMP
ComInitBalancer::PreUninitialize(DWORD reference_count) {
  if (reference_count == 1 && spy_cookie_.has_value()) {
    // Increase the reference count to prevent premature and unbalanced
    // uninitalization of the COM library.
    ::CoInitializeEx(nullptr, co_init_);
  }
  return S_OK;
}

IFACEMETHODIMP
ComInitBalancer::PostUninitialize(DWORD new_reference_count) {
  reference_count_ = new_reference_count;
  return S_OK;
}

}  // namespace internal
}  // namespace win
}  // namespace base
