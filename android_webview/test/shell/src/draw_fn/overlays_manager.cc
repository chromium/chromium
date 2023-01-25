// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/test/shell/src/draw_fn/overlays_manager.h"

#include "android_webview/public/browser/draw_fn.h"
#include "android_webview/test/shell/src/draw_fn/allocator.h"
#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

namespace draw_fn {
namespace {

bool AreOverlaysSupported() {
  static bool supported = gfx::SurfaceControl::IsSupported() &&
                          (base::android::BuildInfo::GetInstance()->sdk_int() >=
                           base::android::SDK_VERSION_S);

  return supported;
}

OverlaysManager::ScopedCurrentFunctorCall* g_current_functor_call = nullptr;

}  // namespace

class OverlaysManager::ScopedCurrentFunctorCall {
 public:
  ScopedCurrentFunctorCall(FunctorData& functor, ANativeWindow* native_window)
      : functor_(functor), native_window_(native_window) {
    DCHECK(!g_current_functor_call);
    g_current_functor_call = this;
  }

  ~ScopedCurrentFunctorCall() {
    DCHECK_EQ(g_current_functor_call, this);
    g_current_functor_call = nullptr;
  }

  static ASurfaceControl* GetSurfaceControlFn() {
    DCHECK(g_current_functor_call);
    DCHECK(AreOverlaysSupported());
    return g_current_functor_call->GetSurfaceControl();
  }

  static void MergeTransactionFn(ASurfaceTransaction* transaction) {
    DCHECK(g_current_functor_call);
    DCHECK(AreOverlaysSupported());
    return g_current_functor_call->MergeTransaction(transaction);
  }

 private:
  ASurfaceControl* GetSurfaceControl() {
    if (!functor_->overlay_surface) {
      DCHECK(native_window_);
      functor_->overlay_surface =
          base::MakeRefCounted<gfx::SurfaceControl::Surface>(native_window_,
                                                             "webview_root");
    }

    return functor_->overlay_surface->surface();
  }

  void MergeTransaction(ASurfaceTransaction* transaction) {
    gfx::SurfaceControl::ApplyTransaction(transaction);
  }

  const raw_ref<FunctorData, DanglingUntriaged> functor_;
  raw_ptr<ANativeWindow> native_window_;
};

template <typename T>
void SetDrawParams(T& params, bool has_window) {
  params.overlays_mode = (AreOverlaysSupported() && has_window)
                             ? AW_DRAW_FN_OVERLAYS_MODE_ENABLED
                             : AW_DRAW_FN_OVERLAYS_MODE_DISABLED;
  params.get_surface_control =
      OverlaysManager::ScopedCurrentFunctorCall::GetSurfaceControlFn;
  params.merge_transaction =
      OverlaysManager::ScopedCurrentFunctorCall::MergeTransactionFn;
}

OverlaysManager::ScopedDraw::ScopedDraw(OverlaysManager& manager,
                                        FunctorData& functor,
                                        AwDrawFn_DrawGLParams& params)
    : scoped_functor_call_(std::make_unique<ScopedCurrentFunctorCall>(
          functor,
          manager.native_window_.a_native_window())) {
  SetDrawParams(params, !!manager.native_window_);
}

OverlaysManager::ScopedDraw::ScopedDraw(OverlaysManager& manager,
                                        FunctorData& functor,
                                        AwDrawFn_DrawVkParams& params)
    : scoped_functor_call_(std::make_unique<ScopedCurrentFunctorCall>(
          functor,
          manager.native_window_.a_native_window())) {
  SetDrawParams(params, !!manager.native_window_);
}

OverlaysManager::ScopedDraw::~ScopedDraw() = default;

OverlaysManager::OverlaysManager() = default;
OverlaysManager::~OverlaysManager() = default;

void OverlaysManager::RemoveOverlays(FunctorData& functor) {
  if (functor.overlay_surface) {
    ScopedCurrentFunctorCall current_call(functor, nullptr);
    AwDrawFn_RemoveOverlaysParams params = {
        .version = kAwDrawFnVersion,
        .merge_transaction = ScopedCurrentFunctorCall::MergeTransactionFn};
    functor.functor_callbacks->remove_overlays(functor.functor, functor.data,
                                               &params);
    functor.overlay_surface.reset();
  }
}

void OverlaysManager::SetSurface(
    int current_functor,
    JNIEnv* env,
    const base::android::JavaRef<jobject>& surface) {
  if (java_surface_.j_surface().obj() == surface.obj()) {
    return;
  }

  if (native_window_) {
    if (current_functor)
      RemoveOverlays(Allocator::Get()->get(current_functor));
    native_window_ = nullptr;
  }

  java_surface_ = gl::ScopedJavaSurface(surface, /*auto_release=*/false);
  if (java_surface_.IsValid()) {
    native_window_ = gl::ScopedANativeWindow(java_surface_);
  }
}
}  // namespace draw_fn
