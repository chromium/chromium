// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_FUCHSIA_FIDLGEN_JS_RUNTIME_ZIRCON_H_
#define BUILD_FUCHSIA_FIDLGEN_JS_RUNTIME_ZIRCON_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "v8/include/v8.h"

namespace fidljs {

class WaitPromiseImpl;

// A WaitSet is associated with each Isolate and represents all outstanding
// waits that are queued on the dispatcher.
//
// If the wait completes normally, the contained promise is resolved, the
// WaitPromiseImpl is marked as completed, and then deleted (by removing it from
// the pending set).
//
// If the caller shuts down with outstanding waits pending, the asynchronous
// waits are canceled by clearing the set (which deletes all the
// WaitPromiseImpls). If a WaitPromiseImpl has not completed when it is
// destroyed, it cancels the outstanding wait in its destructor.
//
// WaitPromiseImpl is responsible for resolving or rejecting promises. If the
// object was created, but a wait never started it will not have been added to
// the wait set, and so will reject the promise immediately. Otherwise, the
// promise will be resolved or rejected when the asynchronous wait is signaled
// or canceled.
using WaitSet =
    base::flat_set<std::unique_ptr<WaitPromiseImpl>, base::UniquePtrComparator>;

class ZxBindings {
 public:
  // Adds Zircon APIs bindings to |global|, for use by JavaScript callers.
  ZxBindings(v8::Isolate* isolate, v8::Local<v8::Object> global);

  // Cleans up attached storage in the isolate added by the bindings, and
  // cancels any pending asynchronous requests. It is important this this be
  // done before the v8 context is torn down.
  ~ZxBindings();

 private:
  v8::Isolate* const isolate_;
  std::unique_ptr<WaitSet> wait_set_;

  DISALLOW_COPY_AND_ASSIGN(ZxBindings);
};

}  // namespace fidljs

#endif  // BUILD_FUCHSIA_FIDLGEN_JS_RUNTIME_ZIRCON_H_
