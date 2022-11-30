// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/delayed_task_handle_delegate.h"

#include "base/task/sequence_manager/task_queue_impl.h"

namespace base {
namespace sequence_manager {
namespace internal {

DelayedTaskHandleDelegate::DelayedTaskHandleDelegate(TaskQueueImpl* outer)
    : outer_(outer) {}

DelayedTaskHandleDelegate::~DelayedTaskHandleDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsValid());
}

WeakPtr<DelayedTaskHandleDelegate> DelayedTaskHandleDelegate::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

bool DelayedTaskHandleDelegate::IsValid() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.HasWeakPtrs();
}

void DelayedTaskHandleDelegate::CancelTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsValid())
    return;

  weak_ptr_factory_.InvalidateWeakPtrs();

  // If the task is still inside the heap, then it can be removed directly.
  if (heap_handle_.IsValid())
    outer_->RemoveCancelableTask(heap_handle_);
}

void DelayedTaskHandleDelegate::SetHeapHandle(HeapHandle heap_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(heap_handle.IsValid());
  heap_handle_ = heap_handle;
}

void DelayedTaskHandleDelegate::ClearHeapHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  heap_handle_ = HeapHandle();
}

HeapHandle DelayedTaskHandleDelegate::GetHeapHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return heap_handle_;
}

void DelayedTaskHandleDelegate::WillRunTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsValid());
  // The task must be removed from the heap before running it.
  DCHECK(!heap_handle_.IsValid());
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
