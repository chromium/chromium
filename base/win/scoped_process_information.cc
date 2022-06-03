// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_process_information.h"

#include "base/logging.h"
#include "base/win/scoped_handle.h"

namespace base {
namespace win {

namespace {

// Duplicates source into target, returning true upon success. |target| is
// guaranteed to be untouched in case of failure. Succeeds with no side-effects
// if source is NULL.
bool CheckAndDuplicateHandle(HANDLE source, ScopedHandle* target) {
  if (!source)
    return true;

  HANDLE temp = nullptr;
  if (!::DuplicateHandle(::GetCurrentProcess(), source, ::GetCurrentProcess(),
                         &temp, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    DWORD last_error = ::GetLastError();
    DPLOG(ERROR) << "Failed to duplicate a handle " << last_error;
    ::SetLastError(last_error);
    return false;
  }
  target->Set(temp);
  return true;
}

}  // namespace

ScopedProcessInformation::ScopedProcessInformation() = default;

ScopedProcessInformation::ScopedProcessInformation(
    const PROCESS_INFORMATION& process_info) {
  Set(process_info);
}

ScopedProcessInformation::~ScopedProcessInformation() {
  Close();
}

bool ScopedProcessInformation::IsValid() const {
  return process_id_ || process_handle_.Get() || thread_id_ ||
         thread_handle_.Get();
}

void ScopedProcessInformation::Close() {
  process_handle_.Close();
  thread_handle_.Close();
  process_id_ = 0;
  thread_id_ = 0;
}

void ScopedProcessInformation::Set(const PROCESS_INFORMATION& process_info) {
  if (IsValid())
    Close();

  process_handle_.Set(process_info.hProcess);
  thread_handle_.Set(process_info.hThread);
  process_id_ = process_info.dwProcessId;
  thread_id_ = process_info.dwThreadId;
}

bool ScopedProcessInformation::DuplicateFrom(
    const ScopedProcessInformation& other) {
  DCHECK(!IsValid()) << "target ScopedProcessInformation must be NULL";
  DCHECK(other.IsValid()) << "source ScopedProcessInformation must be valid";

  if (CheckAndDuplicateHandle(other.process_handle(), &process_handle_) &&
      CheckAndDuplicateHandle(other.thread_handle(), &thread_handle_)) {
    process_id_ = other.process_id();
    thread_id_ = other.thread_id();
    return true;
  }

  return false;
}

PROCESS_INFORMATION ScopedProcessInformation::Take() {
  PROCESS_INFORMATION process_information = {};
  process_information.hProcess = process_handle_.Take();
  process_information.hThread = thread_handle_.Take();
  process_information.dwProcessId = process_id();
  process_information.dwThreadId = thread_id();
  process_id_ = 0;
  thread_id_ = 0;

  return process_information;
}

HANDLE ScopedProcessInformation::TakeProcessHandle() {
  process_id_ = 0;
  return process_handle_.Take();
}

HANDLE ScopedProcessInformation::TakeThreadHandle() {
  thread_id_ = 0;
  return thread_handle_.Take();
}

}  // namespace win
}  // namespace base
