// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/test/shell/src/draw_fn/allocator.h"

#include "android_webview/public/browser/draw_fn.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"

namespace draw_fn {

namespace {

bool g_use_vulkan = false;

AwDrawFnRenderMode QueryRenderMode() {
  return g_use_vulkan ? AW_DRAW_FN_RENDER_MODE_VULKAN
                      : AW_DRAW_FN_RENDER_MODE_OPENGL_ES;
}

int CreateFunctor(void* data, AwDrawFnFunctorCallbacks* functor_callbacks) {
  NOTREACHED();
}

int CreateFunctor_v3(void* data,
                     int version,
                     AwDrawFnFunctorCallbacks* functor_callbacks) {
  DCHECK_GE(version, 3);
  return Allocator::Get()->allocate(data, functor_callbacks);
}

void ReleaseFunctor(int functor) {
  Allocator::Get()->MarkReleasedByFunctor(functor);
}

void ReportRenderingThreads(int functor, const pid_t* thread_ids, size_t size) {
}

}  // namespace

void SetDrawFnUseVulkan(bool use_vulkan) {
  g_use_vulkan = use_vulkan;
}

AwDrawFnFunctionTable* GetDrawFnFunctionTable() {
  static AwDrawFnFunctionTable table{
      kAwDrawFnVersion, &QueryRenderMode,  &CreateFunctor,
      &ReleaseFunctor,  &CreateFunctor_v3, &ReportRenderingThreads};
  return &table;
}

FunctorData::FunctorData() = default;
FunctorData::~FunctorData() = default;
FunctorData::FunctorData(FunctorData&&) = default;
FunctorData& FunctorData::operator=(FunctorData&&) = default;

FunctorData::FunctorData(int functor,
                         void* data,
                         AwDrawFnFunctorCallbacks* functor_callbacks)
    : functor(functor), data(data), functor_callbacks(functor_callbacks) {}

// static
Allocator* Allocator::Get() {
  static base::NoDestructor<Allocator> map;
  return map.get();
}

Allocator::Allocator() = default;
Allocator::~Allocator() = default;

int Allocator::allocate(void* data,
                        AwDrawFnFunctorCallbacks* functor_callbacks) {
  base::AutoLock lock(lock_);
  int functor = next_functor_++;
  map_.emplace(functor, FunctorData(functor, data, functor_callbacks));
  return functor;
}

FunctorData& Allocator::get(int functor) {
  base::AutoLock lock(lock_);
  auto itr = map_.find(functor);
  CHECK(itr != map_.end());
  return itr->second;
}

void Allocator::MarkReleasedByFunctor(int functor) {
  base::AutoLock lock(lock_);
  auto itr = map_.find(functor);
  CHECK(itr != map_.end());
  DCHECK(!itr->second.released_by_functor);
  itr->second.released_by_functor = true;
  MaybeReleaseFunctorAlreadyLocked(functor);
}

void Allocator::MarkReleasedByManager(int functor) {
  base::AutoLock lock(lock_);
  auto itr = map_.find(functor);
  CHECK(itr != map_.end());
  DCHECK(!itr->second.released_by_manager);
  MaybeReleaseFunctorAlreadyLocked(functor);
}

void Allocator::MaybeReleaseFunctorAlreadyLocked(int functor) {
  lock_.AssertAcquired();
  auto itr = map_.find(functor);
  const FunctorData& data = itr->second;
  if (data.released_by_functor && data.released_by_manager) {
    data.functor_callbacks->on_destroyed(data.functor, data.data);
    map_.erase(itr);
  }
}

}  // namespace draw_fn
