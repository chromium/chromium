// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"

#if DCHECK_IS_ON()
#include <ostream>

#include "base/debug/stack_trace.h"
#endif

namespace base::internal {

WeakReference::Flag::Flag() {
  // Flags only become bound when checked for validity, or invalidated,
  // so that we can check that later validity/invalidation operations on
  // the same Flag take place on the same sequenced thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void WeakReference::Flag::Invalidate() {
  // The flag being invalidated with a single ref implies that there are no
  // weak pointers in existence. Allow deletion on other thread in this case.
#if DCHECK_IS_ON()
  std::unique_ptr<debug::StackTrace> bound_at;
  DCHECK(sequence_checker_.CalledOnValidSequence(&bound_at) || HasOneRef())
      << "WeakPtrs must be invalidated on the same sequenced thread as where "
      << "they are bound.\n"
      << (bound_at ? "This was bound at:\n" + bound_at->ToString() : "")
      << "Check failed at:";
#endif
  invalidated_.Set();
}

bool WeakReference::Flag::IsValid() const {
  // WeakPtrs must be checked on the same sequenced thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !invalidated_.IsSet();
}

bool WeakReference::Flag::MaybeValid() const {
  return !invalidated_.IsSet();
}

#if DCHECK_IS_ON()
void WeakReference::Flag::DetachFromSequence() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void WeakReference::Flag::BindToCurrentSequence() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}
#endif

WeakReference::Flag::~Flag() = default;

WeakReference::WeakReference() = default;
WeakReference::WeakReference(const scoped_refptr<Flag>& flag) : flag_(flag) {}
WeakReference::~WeakReference() = default;

WeakReference::WeakReference(const WeakReference& other) = default;
WeakReference& WeakReference::operator=(const WeakReference& other) = default;

WeakReference::WeakReference(WeakReference&& other) noexcept = default;
WeakReference& WeakReference::operator=(WeakReference&& other) noexcept =
    default;

void WeakReference::Reset() {
  flag_ = nullptr;
}

bool WeakReference::IsValid() const {
  return flag_ && flag_->IsValid();
}

bool WeakReference::MaybeValid() const {
  return flag_ && flag_->MaybeValid();
}

WeakReferenceOwner::WeakReferenceOwner()
    : flag_(MakeRefCounted<WeakReference::Flag>()) {}

WeakReferenceOwner::~WeakReferenceOwner() {
  flag_->Invalidate();
}

WeakReference WeakReferenceOwner::GetRef() const {
#if DCHECK_IS_ON()
  // If we hold the last reference to the Flag then detach the SequenceChecker.
  if (!HasRefs())
    flag_->DetachFromSequence();
#endif

  return WeakReference(flag_);
}

void WeakReferenceOwner::Invalidate() {
  flag_->Invalidate();
  flag_ = MakeRefCounted<WeakReference::Flag>();
}

void WeakReferenceOwner::BindToCurrentSequence() {
#if DCHECK_IS_ON()
  flag_->BindToCurrentSequence();
#endif
}

WeakPtrFactoryBase::WeakPtrFactoryBase(uintptr_t ptr) : ptr_(ptr) {
  DCHECK(ptr_);
}

WeakPtrFactoryBase::~WeakPtrFactoryBase() {
  ptr_ = 0;
}

}  // namespace base::internal
