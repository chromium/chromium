// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_SCOPED_SERVICE_BINDING_BATCH_H_
#define BASE_ANDROID_SCOPED_SERVICE_BINDING_BATCH_H_

#include "base/android/scoped_java_ref.h"
#include "base/base_export.h"
#include "base/memory/stack_allocated.h"

namespace base::android {

// ScopedServiceBindingBatch is used to batch up service binding requests.
//
//
// This is a C++ wrapper for the ScopedServiceBindingBatch Java class. This
// class follows RAII to ensure that batch updates are started and stopped
// correctly.
//
// When a ScopedServiceBindingBatch is created, it begins a batch update on the
// process launcher thread. When the ScopedServiceBindingBatch is destroyed, it
// ends the batch update. ScopedServiceBindingBatch supports nested batch
// updates. If the batch update count drops to 0, the binding request queue is
// flushed.
//
// ScopedServiceBindingBatch must be created on the main thread to ensure that
// nested batch window does not partially overlap. The batch open/end events are
// dispatched to the process launcher thread and counter is
// incremented/decremented on the launcher thread.
//
// While it is in batch mode, BindService will queue up binding requests. When
// the batch is over, the queue is flushed.
class BASE_EXPORT ScopedServiceBindingBatch {
  // Disallow allocation on heap to enforce RAII usage. This is to prevent
  // overlapping batch updates partially which can cause too long batch window.
  STACK_ALLOCATED();

 public:
  ScopedServiceBindingBatch();
  ~ScopedServiceBindingBatch();

  ScopedServiceBindingBatch(const ScopedServiceBindingBatch&) = delete;
  ScopedServiceBindingBatch& operator=(const ScopedServiceBindingBatch&) =
      delete;
  // Disable move constructor and move assignment operator to ensure that
  // scopes are not interleaved, but just cleanly nested.
  ScopedServiceBindingBatch(ScopedServiceBindingBatch&&) = delete;
  ScopedServiceBindingBatch& operator=(ScopedServiceBindingBatch&&) = delete;

 private:
  base::android::ScopedJavaLocalRef<jobject> java_object_;
};

}  // namespace base::android

#endif  // BASE_ANDROID_SCOPED_SERVICE_BINDING_BATCH_H_
