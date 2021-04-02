// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_HANDLE_VERIFIER_H_
#define BASE_WIN_SCOPED_HANDLE_VERIFIER_H_

#include "base/win/windows_types.h"

#include <memory>
#include <unordered_map>

#include "base/base_export.h"
#include "base/debug/stack_trace.h"
#include "base/hash/hash.h"
#include "base/synchronization/lock_impl.h"
#include "base/threading/thread_local.h"

namespace base {
namespace win {
namespace internal {

struct HandleHash {
  size_t operator()(const HANDLE& handle) const {
    return base::FastHash(as_bytes(make_span(&handle, 1)));
  }
};

struct ScopedHandleVerifierInfo {
  ScopedHandleVerifierInfo(const void* owner,
                           const void* pc1,
                           const void* pc2,
                           std::unique_ptr<debug::StackTrace> stack,
                           DWORD thread_id);
  ~ScopedHandleVerifierInfo();

  ScopedHandleVerifierInfo(const ScopedHandleVerifierInfo&) = delete;
  ScopedHandleVerifierInfo& operator=(const ScopedHandleVerifierInfo&) = delete;
  ScopedHandleVerifierInfo(ScopedHandleVerifierInfo&&) noexcept;
  ScopedHandleVerifierInfo& operator=(ScopedHandleVerifierInfo&&) noexcept;

  const void* owner;
  const void* pc1;
  const void* pc2;
  std::unique_ptr<debug::StackTrace> stack;
  DWORD thread_id;
};

// Implements the actual object that is verifying handles for this process.
// The active instance is shared across the module boundary but there is no
// way to delete this object from the wrong side of it (or any side, actually).
// We need [[clang::lto_visibility_public]] because instances of this class are
// passed across module boundaries. This means different modules must have
// compatible definitions of the class even when whole program optimization is
// enabled - which is what this attribute accomplishes. The pragma stops MSVC
// from emitting an unrecognized attribute warning.
#pragma warning(push)
#pragma warning(disable : 5030)
class [[clang::lto_visibility_public]] ScopedHandleVerifier {
#pragma warning(pop)
 public:
  explicit ScopedHandleVerifier(bool enabled);

  // Retrieves the current verifier.
  static ScopedHandleVerifier* Get();

  // The methods required by HandleTraits. They are virtual because we need to
  // forward the call execution to another module, instead of letting the
  // compiler call the version that is linked in the current module.
  virtual bool CloseHandle(HANDLE handle);
  virtual void StartTracking(HANDLE handle, const void* owner, const void* pc1,
                             const void* pc2);
  virtual void StopTracking(HANDLE handle, const void* owner, const void* pc1,
                            const void* pc2);
  virtual void Disable();
  virtual void OnHandleBeingClosed(HANDLE handle);
  virtual HMODULE GetModule() const;

 private:
  ~ScopedHandleVerifier();  // Not implemented.

  void StartTrackingImpl(HANDLE handle, const void* owner, const void* pc1,
                         const void* pc2);
  void StopTrackingImpl(HANDLE handle, const void* owner, const void* pc1,
                        const void* pc2);
  void OnHandleBeingClosedImpl(HANDLE handle);

  static base::internal::LockImpl* GetLock();
  static void InstallVerifier();
  static void ThreadSafeAssignOrCreateScopedHandleVerifier(
      ScopedHandleVerifier * existing_verifier, bool enabled);

  base::debug::StackTrace creation_stack_;
  bool enabled_;
  base::ThreadLocalBoolean closing_;
  base::internal::LockImpl* lock_;
  std::unordered_map<HANDLE, ScopedHandleVerifierInfo, HandleHash> map_;
  DISALLOW_COPY_AND_ASSIGN(ScopedHandleVerifier);
};

// This testing function returns the module that the HandleVerifier concrete
// implementation was instantiated in.
BASE_EXPORT HMODULE GetHandleVerifierModuleForTesting();

}  // namespace internal
}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SCOPED_HANDLE_VERIFIER_H_
