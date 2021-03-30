// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include <limits>

#include "base/immediate_crash.h"

namespace base {

static constexpr ProcessHandle kCurrentProcessHandle =
    std::numeric_limits<ProcessHandle>::max();

Process::Process(ProcessHandle handle) : process_(handle) {
  DCHECK(handle == kNullProcessHandle || handle == kCurrentProcessHandle);
}

Process::Process(Process&& other) : process_(other.process_) {
  other.Close();
}

Process::~Process() = default;

Process& Process::operator=(Process&& other) {
  process_ = other.process_;
  other.Close();
  return *this;
}

// static
Process Process::Current() {
  return Process(kCurrentProcessHandle);
}

// static
Process Process::Open(ProcessId pid) {
  return Process(pid);
}

// static
Process Process::OpenWithExtraPrivileges(ProcessId pid) {
  return Process(pid);
}

// static
void Process::TerminateCurrentProcessImmediately(int exit_code) {
  // This method is marked noreturn, so we crash rather than just provide an
  // empty stub implementation.
  IMMEDIATE_CRASH();
}

bool Process::IsValid() const {
  return process_ != kNullProcessHandle;
}

ProcessHandle Process::Handle() const {
  return process_;
}

Process Process::Duplicate() const {
  return Process(process_);
}

ProcessHandle Process::Release() {
  ProcessHandle handle = process_;
  Close();
  return handle;
}

ProcessId Process::Pid() const {
  return process_;
}

Time Process::CreationTime() const {
  return Time();
}

bool Process::is_current() const {
  return Handle() == kCurrentProcessHandle;
}

void Process::Close() {
  process_ = kNullProcessHandle;
}

bool Process::WaitForExit(int* exit_code) const {
  return false;
}

bool Process::WaitForExitWithTimeout(TimeDelta timeout, int* exit_code) const {
  return false;
}

void Process::Exited(int exit_code) const {}

bool Process::IsProcessBackgrounded() const {
  return false;
}

bool Process::SetProcessBackgrounded(bool value) {
  return false;
}

int Process::GetPriority() const {
  return -1;
}

}  // namespace base
