// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_ALLOCATOR_H_
#define ANDROID_WEBVIEW_TEST_SHELL_SRC_DRAW_FN_ALLOCATOR_H_

#include "android_webview/public/browser/draw_fn.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/android/android_surface_control_compat.h"

namespace draw_fn {

void SetDrawFnUseVulkan(bool use_vulkan);

AwDrawFnFunctionTable* GetDrawFnFunctionTable();

struct FunctorData {
  FunctorData();
  FunctorData(int functor,
              void* data,
              AwDrawFnFunctorCallbacks* functor_callbacks);
  ~FunctorData();
  FunctorData(FunctorData&&);
  FunctorData& operator=(FunctorData&&);

  FunctorData(const FunctorData&) = delete;
  FunctorData& operator=(const FunctorData&) = delete;

  int functor = 0;
  raw_ptr<void> data = nullptr;
  raw_ptr<AwDrawFnFunctorCallbacks> functor_callbacks = nullptr;
  bool released_by_functor = false;
  bool released_by_manager = false;
  scoped_refptr<gfx::SurfaceControl::Surface> overlay_surface;
};

class Allocator {
 public:
  static Allocator* Get();

  int allocate(void* data, AwDrawFnFunctorCallbacks* functor_callbacks);
  FunctorData& get(int functor);
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
