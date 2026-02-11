// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <array>

#include "base/bits.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/trace_event/trace_event.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/scheduler.h"
#endif

namespace base {
namespace {

MessagePumpType GetCurrentMessagePumpType() {
  // CurrentIOThread::IsSet() and CurrentUIThread::IsSet() can't both be set at
  // the same time, so there's no precedence to worry about: both
  // CurrentIOThread::IsSet() and CurrentUIThread::IsSet() rely on
  // GetCurrentSequenceManagerImpl(), which returns the single SequenceManager
  // instance bound to the current thread.
  if (CurrentIOThread::IsSet()) {
    return MessagePumpType::IO;
  } else if (CurrentUIThread::IsSet()) {
    return MessagePumpType::UI;
  }
  return MessagePumpType::DEFAULT;
}

internal::ThreadTypeManager* GetThreadTypeManager() {
  constinit thread_local internal::ThreadTypeManager thread_type_manager;
  return &thread_type_manager;
}

}  // namespace

void PlatformThreadId::WriteIntoTrace(perfetto::TracedValue&& context) const {
  perfetto::WriteIntoTracedValue(std::move(context), value_);
}

// static
void PlatformThreadBase::SetCurrentThreadType(ThreadType thread_type) {
  GetThreadTypeManager()->SetDefault(thread_type);
}

// static
ThreadType PlatformThreadBase::GetCurrentThreadType() {
  return GetThreadTypeManager()->GetCurrent();
}

// static
std::optional<TimeDelta> PlatformThreadBase::GetThreadLeewayOverride() {
#if BUILDFLAG(IS_FUCHSIA)
  // On Fuchsia, all audio threads run with the CPU scheduling profile that uses
  // an interval of |kAudioSchedulingPeriod|. Using the default leeway may lead
  // to some tasks posted to audio threads to be executed too late (see
  // http://crbug.com/1368858).
  if (GetCurrentThreadType() == ThreadType::kRealtimeAudio) {
    return kAudioSchedulingPeriod;
  }
#endif
  return std::nullopt;
}

// static
void PlatformThreadBase::SetNameCommon(const std::string& name) {
  ThreadIdNameManager::GetInstance()->SetName(name);
}

// static
bool PlatformThreadBase::CurrentThreadHasLeases() {
  return GetThreadTypeManager()->HasLeases();
}

PlatformThreadBase::RaiseThreadTypeLease::RaiseThreadTypeLease(
    ThreadType thread_type)
    : RaiseThreadTypeLease(thread_type, GetThreadTypeManager()) {}

PlatformThreadBase::RaiseThreadTypeLease::RaiseThreadTypeLease(
    ThreadType thread_type,
    internal::ThreadTypeManager* manager)
    : leased_thread_type_(thread_type), manager_(manager) {
  // The lease system is currently not fully compatible with
  // ScopedBoostablePriority since they both control the thread type without
  // coordination and in slightly different ways. Creating a
  // ScopedBoostablePriority while a lease is active works, but not the other
  // way around.
  //
  // TODO(crbug.com/483622914): consider supporting both in a more relaxed way.
  DCHECK(!ScopedBoostablePriority::CurrentThreadHasScope());
  manager_->AcquireRaiseLease(thread_type);
}

PlatformThreadBase::RaiseThreadTypeLease::~RaiseThreadTypeLease() {
  // The lease system is currently not fully compatible with
  // ScopedBoostablePriority since they both control the thread type without
  // coordination and in slightly different ways. Creating a
  // ScopedBoostablePriority while a lease is active works, but not the other
  // way around.
  //
  // TODO(crbug.com/483622914): consider supporting both in a more relaxed way.
  DCHECK(!ScopedBoostablePriority::CurrentThreadHasScope());
  manager_->DropRaiseLease(leased_thread_type_);
}

namespace internal {

void ThreadTypeManager::SetDefault(ThreadType type) {
  CHECK_LE(type, ThreadType::kMaxValue);
  default_thread_type_ = type;
  MaybeUpdate();
}

ThreadType ThreadTypeManager::GetCurrent() const {
  return effective_thread_type_.value_or(ThreadType::kDefault);
}

void ThreadTypeManager::MaybeUpdate() {
  auto highest_lease = raise_leases_.GetHighestLease();
  ThreadType type;
  if (!highest_lease.has_value() && !default_thread_type_.has_value()) {
    type = ThreadType::kDefault;
  } else {
    type = std::max(highest_lease.value_or(ThreadType::kBackground),
                    default_thread_type_.value_or(ThreadType::kBackground));
  }

  if (type != effective_thread_type_) {
    effective_thread_type_ = type;
    SetCurrentThreadTypeImpl(effective_thread_type_.value(),
                             GetCurrentMessagePumpType());
  }
}

void ThreadTypeManager::AcquireRaiseLease(ThreadType type) {
  CHECK_LE(type, ThreadType::kMaxValue);
  raise_leases_.Acquire(type);
  MaybeUpdate();
}

void ThreadTypeManager::DropRaiseLease(ThreadType type) {
  CHECK_LE(type, ThreadType::kMaxValue);
  raise_leases_.Drop(type);
  MaybeUpdate();
}

void ThreadTypeManager::RaiseLeases::Acquire(ThreadType thread_type) {
  // TODO(crbug.com/470337728): consider using base::EnumSet.
  auto type = static_cast<uint32_t>(thread_type);
  leases[type]++;
  bitmask |= (1u << type);
}

void ThreadTypeManager::RaiseLeases::Drop(ThreadType thread_type) {
  // TODO(crbug.com/470337728): consider using base::EnumSet.
  auto type = static_cast<uint32_t>(thread_type);
  leases[type]--;
  if (leases[type] == 0) {
    bitmask &= ~(1u << type);
  }
}

std::optional<ThreadType> ThreadTypeManager::RaiseLeases::GetHighestLease()
    const {
  if (bitmask == 0) {
    return std::nullopt;
  }
  return static_cast<ThreadType>(base::bits::Log2Floor(bitmask));
}

void ThreadTypeManager::SetCurrentThreadTypeImpl(
    ThreadType thread_type,
    MessagePumpType pump_type_hint) {
  base::internal::SetCurrentThreadTypeImpl(thread_type, pump_type_hint);
}

bool ThreadTypeManager::HasLeases() const {
  return raise_leases_.GetHighestLease().has_value();
}
}  // namespace internal
}  // namespace base
