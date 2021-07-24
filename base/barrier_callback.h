// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BARRIER_CALLBACK_H_
#define BASE_BARRIER_CALLBACK_H_

#include <type_traits>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace base {

namespace {

template <typename T>
class BarrierCallbackInfo {
 public:
  BarrierCallbackInfo(size_t num_callbacks,
                      OnceCallback<void(std::vector<T>)> done_callback)
      : num_callbacks_left_(num_callbacks),
        done_callback_(std::move(done_callback)) {
    results_.reserve(num_callbacks);
  }

  void Run(T t) LOCKS_EXCLUDED(mutex_) {
    base::ReleasableAutoLock lock(&mutex_);
    DCHECK_NE(num_callbacks_left_, 0U);
    results_.push_back(std::move(t));
    --num_callbacks_left_;

    if (num_callbacks_left_ == 0) {
      std::vector<T> results = std::move(results_);
      lock.Release();
      std::move(done_callback_).Run(std::move(results));
    }
  }

 private:
  Lock mutex_;
  size_t num_callbacks_left_ GUARDED_BY(mutex_);
  std::vector<T> results_ GUARDED_BY(mutex_);
  OnceCallback<void(std::vector<T>)> done_callback_;
};

}  // namespace

// BarrierCallback<T> is an analog of BarrierClosure for which each `Run()`
// invocation takes a `T` as an argument. After `num_callbacks` such
// invocations, BarrierCallback invokes `Run()` on its `done_callback`, passing
// the vector of `T`s as an argument. (The ordering of the vector is
// unspecified.)
//
// It is an error to call `BarrierCallback` with `num_callbacks` equal to 0.
//
// BarrierCallback is thread-safe - the internals are protected by a
// `base::Lock`. `done_callback` will be run on the thread that calls the final
// Run() on the returned callbacks, or the thread that constructed the
// BarrierCallback (in the case where `num_callbacks` is 0).
//
// `done_callback` is also cleared on the thread that runs it (by virtue of
// being a OnceCallback).
template <typename T>
RepeatingCallback<void(T)> BarrierCallback(
    size_t num_callbacks,
    OnceCallback<void(std::vector<T>)> done_callback) {
  CHECK_GT(num_callbacks, 0U);

  return BindRepeating(&BarrierCallbackInfo<T>::Run,
                       std::make_unique<BarrierCallbackInfo<T>>(
                           num_callbacks, std::move(done_callback)));
}

}  // namespace base

#endif  // BASE_BARRIER_CALLBACK_H_
