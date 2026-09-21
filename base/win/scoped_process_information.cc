// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_process_information.h"

#include <utility>

#include "base/check.h"
#include "base/win/scoped_handle.h"

namespace base {
namespace win {

ScopedProcessInformation::ScopedProcessInformation() = default;

ScopedProcessInformation::ScopedProcessInformation(
    const PROCESS_INFORMATION& process_info) {
  Set(process_info);
}

ScopedProcessInformation::ScopedProcessInformation(ScopedProcessInformation&&) =
    default;

ScopedProcessInformation& ScopedProcessInformation::operator=(
    ScopedProcessInformation&&) = default;

ScopedProcessInformation::~ScopedProcessInformation() {
  Close();
}

bool ScopedProcessInformation::IsValid() const {
  return process_id_ || process_handle_.get() || thread_id_ ||
         thread_handle_.get();
}

void ScopedProcessInformation::Close() {
  process_handle_.Close();
  thread_handle_.Close();
  process_id_ = 0;
  thread_id_ = 0;
}

void ScopedProcessInformation::Set(const PROCESS_INFORMATION& process_info) {
  if (IsValid()) {
    Close();
  }

  process_handle_.Set(process_info.hProcess);
  thread_handle_.Set(process_info.hThread);
  process_id_ = process_info.dwProcessId;
  thread_id_ = process_info.dwThreadId;
}

bool ScopedProcessInformation::DuplicateFrom(
    const ScopedProcessInformation& other) {
  DCHECK(!IsValid()) << "target ScopedProcessInformation must be NULL";
  DCHECK(other.IsValid()) << "source ScopedProcessInformation must be valid";

  ScopedHandle duplicate_process = other.process_handle_.Duplicate();
  if (other.process_handle_.is_valid() && !duplicate_process.is_valid()) {
    return false;
  }

  ScopedHandle duplicate_thread = other.thread_handle_.Duplicate();
  if (other.thread_handle_.is_valid() && !duplicate_thread.is_valid()) {
    return false;
  }

  process_handle_ = std::move(duplicate_process);
  thread_handle_ = std::move(duplicate_thread);
  process_id_ = other.process_id();
  thread_id_ = other.thread_id();
  return true;
}

PROCESS_INFORMATION ScopedProcessInformation::Take() {
  PROCESS_INFORMATION process_information = {};
  process_information.hProcess = process_handle_.release();
  process_information.hThread = thread_handle_.release();
  process_information.dwProcessId = process_id();
  process_information.dwThreadId = thread_id();
  process_id_ = 0;
  thread_id_ = 0;

  return process_information;
}

HANDLE ScopedProcessInformation::TakeProcessHandle() {
  process_id_ = 0;
  return process_handle_.release();
}

HANDLE ScopedProcessInformation::TakeThreadHandle() {
  thread_id_ = 0;
  return thread_handle_.release();
}

}  // namespace win
}  // namespace base
