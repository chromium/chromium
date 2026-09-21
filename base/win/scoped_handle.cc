// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_handle.h"

#include <windows.h>

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/win/scoped_handle_verifier.h"
#include "base/win/win_util.h"
#include "base/win/windows_types.h"

namespace base {
namespace win {

using base::win::internal::ScopedHandleVerifier;

std::ostream& operator<<(std::ostream& os, HandleOperation operation) {
  switch (operation) {
    case HandleOperation::kHandleAlreadyTracked:
      return os << "Handle Already Tracked";
    case HandleOperation::kCloseHandleNotTracked:
      return os << "Closing an untracked handle";
    case HandleOperation::kCloseHandleNotOwner:
      return os << "Closing a handle owned by something else";
    case HandleOperation::kCloseHandleHook:
      return os << "CloseHandleHook validation failure";
    case HandleOperation::kDuplicateHandleHook:
      return os << "DuplicateHandleHook validation failure";
  }
}

// Static.
bool HandleTraits::CloseHandle(HANDLE handle) {
  return ScopedHandleVerifier::Get()->CloseHandle(handle);
}

// Static.
HANDLE HandleTraits::DuplicateHandle(HANDLE handle) {
  // Reject null and pseudo-handles (e.g. INVALID_HANDLE_VALUE /
  // GetCurrentProcess() / GetCurrentThread()) to prevent laundering a failed
  // API return value or pseudo-handle into a real handle with full access.
  // See docs/security/windows-handle-security-guidelines.md.
  CHECK(IsHandleValid(handle));

  HANDLE duplicate = nullptr;
  if (!::DuplicateHandle(::GetCurrentProcess(), handle, ::GetCurrentProcess(),
                         &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    return nullptr;
  }
  CHECK(IsHandleValid(duplicate));
  return duplicate;
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

void OnHandleBeingClosed(HANDLE handle, HandleOperation operation) {
  return ScopedHandleVerifier::Get()->OnHandleBeingClosed(handle, operation);
}

expected<ScopedHandle, NTSTATUS> TakeHandleOfType(
    HANDLE handle,
    std::wstring_view object_type_name) {
  // Invalid and pseudo handle values are rejected by GetObjectTypeName().
  auto type_name = GetObjectTypeName(handle);
  if (!type_name.has_value()) {
    // `handle` is invalid. Return the error to the caller.
    return unexpected(type_name.error());
  }
  // Crash if `handle` is an unexpected type. This represents a dangerous
  // type confusion condition that should never happen.
  base::debug::Alias(&handle);
  CHECK_EQ(*type_name, object_type_name);
  return ScopedHandle(handle);  // Ownership of `handle` goes to the caller.
}

}  // namespace win
}  // namespace base
