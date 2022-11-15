// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_internal.h"

#include "base/check.h"
#include "base/notreached.h"

namespace base {
namespace internal {

namespace {

bool QueryCancellationTraitsForNonCancellables(
    const BindStateBase*,
    BindStateBase::CancellationQueryMode mode) {
  switch (mode) {
    case BindStateBase::IS_CANCELLED:
      return false;
    case BindStateBase::MAYBE_VALID:
      return true;
  }
  NOTREACHED();
}

}  // namespace

void BindStateBaseRefCountTraits::Destruct(const BindStateBase* bind_state) {
  bind_state->destructor_(bind_state);
}

BindStateBase::BindStateBase(InvokeFuncStorage polymorphic_invoke,
                             void (*destructor)(const BindStateBase*))
    : BindStateBase(polymorphic_invoke,
                    destructor,
                    &QueryCancellationTraitsForNonCancellables) {}

BindStateBase::BindStateBase(
    InvokeFuncStorage polymorphic_invoke,
    void (*destructor)(const BindStateBase*),
    bool (*query_cancellation_traits)(const BindStateBase*,
                                      CancellationQueryMode))
    : polymorphic_invoke_(polymorphic_invoke),
      destructor_(destructor),
      query_cancellation_traits_(query_cancellation_traits) {}

BindStateHolder& BindStateHolder::operator=(BindStateHolder&&) noexcept =
    default;

BindStateHolder::BindStateHolder(const BindStateHolder&) = default;

BindStateHolder& BindStateHolder::operator=(const BindStateHolder&) = default;

BindStateHolder::~BindStateHolder() = default;

void BindStateHolder::Reset() {
  bind_state_ = nullptr;
}

bool BindStateHolder::IsCancelled() const {
  DCHECK(bind_state_);
  return bind_state_->IsCancelled();
}

bool BindStateHolder::MaybeValid() const {
  DCHECK(bind_state_);
  return bind_state_->MaybeValid();
}

}  // namespace internal
}  // namespace base
