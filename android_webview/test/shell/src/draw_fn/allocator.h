// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_ALLOCATOR_H_
#define ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_ALLOCATOR_H_

#include "android_webview/public/browser/draw_fn.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace draw_fn {

AwDrawFnFunctionTable* GetDrawFnFunctionTable();

struct FunctorData {
  int functor = 0;
  void* data = nullptr;
  AwDrawFnFunctorCallbacks* functor_callbacks = nullptr;
  bool released_by_functor = false;
  bool released_by_manager = false;
};

class Allocator {
 public:
  static Allocator* Get();

  int allocate(void* data, AwDrawFnFunctorCallbacks* functor_callbacks);
  FunctorData get(int functor);
  void MarkReleasedByFunctor(int functor);
  void MarkReleasedByManager(int functor);

 private:
  friend base::NoDestructor<Allocator>;

  void MaybeReleaseFunctorAlreadyLocked(int functor);

  Allocator();
  ~Allocator();

  base::Lock lock_;
  base::flat_map<int, FunctorData> map_;
  int next_functor_ = 1;
};

}  // namespace draw_fn

#endif  // ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_ALLOCATOR_H_
