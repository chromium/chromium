// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_handle.h"
#include "base/win/scoped_handle_verifier.h"
#include "base/win/windows_types.h"

namespace base {
namespace win {

using base::win::internal::ScopedHandleVerifier;

// Static.
bool HandleTraits::CloseHandle(HANDLE handle) {
  return ScopedHandleVerifier::Get()->CloseHandle(handle);
}

// Static.
void VerifierTraits::StartTracking(HANDLE handle,
                                   const void* owner,
                                   const void* pc1,
                                   const void* pc2) {
  return ScopedHandleVerifier::Get()->StartTracking(handle, owner, pc1, pc2);
}

// Static.
void VerifierTraits::StopTracking(HANDLE handle,
                                  const void* owner,
                                  const void* pc1,
                                  const void* pc2) {
  return ScopedHandleVerifier::Get()->StopTracking(handle, owner, pc1, pc2);
}

void DisableHandleVerifier() {
  return ScopedHandleVerifier::Get()->Disable();
}

void OnHandleBeingClosed(HANDLE handle) {
  return ScopedHandleVerifier::Get()->OnHandleBeingClosed(handle);
}

}  // namespace win
}  // namespace base
