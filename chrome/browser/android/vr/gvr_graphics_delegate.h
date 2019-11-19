// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_GVR_GRAPHICS_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_GVR_GRAPHICS_DELEGATE_H_

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/vr/web_xr_presentation_state.h"
#include "chrome/browser/vr/base_graphics_delegate.h"
#include "chrome/browser/vr/render_info.h"
#include "device/vr/util/sliding_average.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class WaitableEvent;
}

namespace gfx {
class GpuFence;
}

namespace gl {
class GLSurface;
class ScopedJavaSurface;
class SurfaceTexture;
}  // namespace gl

namespace vr {

class GlBrowserInterface;

struct WebVrBounds {
  WebVrBounds(const gfx::RectF& left,
              const gfx::RectF& right,
              const gfx::Size& size)
      : left_bounds(left), right_bounds(right), source_size(size) {}
  gfx::RectF left_bounds;
  gfx::RectF right_bounds;
  gfx::Size source_size;
};

struct Viewport {
  gvr::BufferViewport left;
  gvr::BufferViewport right;

  void SetSourceBufferIndex(int index) {
    left.SetSourceBufferIndex(index);
    right.SetSourceBufferIndex(index);
  }

  void SetSourceUv(const gvr::Rectf& uv) {
    left.SetSourceUv(uv);
    right.SetSourceUv(uv);
  }
};

class GvrGraphicsDelegate : public BaseGraphicsDelegate {
 public:
  using WebXrTokenSignaledCallback =
      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>;
  GvrGraphicsDelegate(GlBrowserInterface* browser,
                      TexturesInitializedCallback textures_initialized_callback,
                      gvr::GvrApi* gvr_api,
                      bool reprojected_rendering,
                      bool pause_content,
                      bool low_density,
                      size_t sliding_time_size);
  ~GvrGraphicsDelegate() override;

  void set_webxr_presentation_state(WebXrPresentationState* webxr) {
    webxr_ = webxr;
  }
  void Init(base::WaitableEvent* gl_surface_created_event,
            base::OnceCallback<gfx::AcceleratedWidget()> callback,
            bool start_in_webxr_mode);
  base::WeakPtr<GvrGraphicsDelegate> GetWeakPtr();

  // GvrSchedulerDelegate communicates with this class through these functions.
  bool DoesSurfacelessRendering() const;
  void RecordFrameTimeTraces() const;
  void SetWebXrBounds(const WebVrBounds& bounds);
  base::TimeDelta GetAcquireTimeAverage() const;
  void OnWebXrFrameAvailable();
  void UpdateViewports();
  bool AcquireGvrFrame(int frame_index);
  void SubmitToGvr(const gfx::Transform& head_pose);
  bool CreateOrResizeWebXrSurface(
      const gfx::Size& size,
      base::RepeatingClosure on_webxr_frame_available);
  void set_webxr_use_shared_buffer_draw(bool use) {
    webxr_use_shared_buffer_draw_ = use;
  }
  bool ResizeForWebXr();
  void ResizeForBrowser();
  gl::SurfaceTexture* webxr_surface_texture() {
    return webxr_surface_texture_.get();
  }
  gfx::Size webxr_surface_size() const { return webxr_surface_size_; }

 private:
  // GraphicsDelegate overrides.
  FovRectangles GetRecommendedFovs() override;
  float GetZNear() override;
  RenderInfo GetRenderInfo(FrameType frame_type,
                           const gfx::Transform& head_pose) override;
  RenderInfo GetOptimizedRenderInfoForFovs(const FovRectangles& fovs) override;
  void InitializeBuffers() override;
  void PrepareBufferForWebXr() override;
  void PrepareBufferForWebXrOverlayElements() override;
  bool IsContentQuadReady() override;
  void PrepareBufferForContentQuadLayer(
      const gfx::Transform& quad_transform) override;
  void PrepareBufferForBrowserUi() override;
  void OnFinishedDrawingBuffer() override;
  void GetWebXrDrawParams(int* texture_id, Transform* uv_transform) override;
  void GetContentQuadDrawParams(Transform* uv_transform,
                                float* border_x,
                                float* border_y) override;
  int GetContentBufferWidth() override;
  void BufferBoundsChanged(const gfx::Size& content_buffer_size,
                           const gfx::Size& overlay_buffer_size) override;
  void ResumeContentRendering() override;
  void SetFrameDumpFilepathBase(std::string& filepath_base) override;
  // End GraphicsDelegate overrides.

  void UIBoundsChanged(int width, int height);

  void InitializeGl(gfx::AcceleratedWidget surface, bool start_in_webxr_mode);
  void InitializeRenderer(bool start_in_webxr_mode);

  void UpdateEyeInfos(const gfx::Transform& head_pose,
                      const Viewport& viewport,
                      const gfx::Size& render_size,
                      RenderInfo* out_render_info);
  void UpdateContentViewportTransforms(const gfx::Transform& head_pose);
  bool WebVrPoseByteIsValid(int pose_index_byte);

  void OnContentFrameAvailable();
  void OnContentOverlayFrameAvailable();
  void OnUiFrameAvailable();

  void WebVrWaitForServerFence();
  void MaybeDumpFrameBufferToDisk();

  WebXrPresentationState* webxr_;

  // samplerExternalOES texture data for WebVR content image.
  int webvr_texture_id_ = 0;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::SurfaceTexture> content_surface_texture_;
  scoped_refptr<gl::SurfaceTexture> content_overlay_surface_texture_;
  scoped_refptr<gl::SurfaceTexture> ui_surface_texture_;
  scoped_refptr<gl::SurfaceTexture> webxr_surface_texture_;
  float webvr_surface_texture_uv_transform_[16];
  std::unique_ptr<gl::ScopedJavaSurface> content_surface_;
  std::unique_ptr<gl::ScopedJavaSurface> ui_surface_;
  std::unique_ptr<gl::ScopedJavaSurface> content_overlay_surface_;

  gvr::GvrApi* gvr_api_;
  gvr::BufferViewportList viewport_list_;
  Viewport main_viewport_;
  Viewport webvr_viewport_;
  Viewport webvr_overlay_viewport_;
  Viewport content_underlay_viewport_;
  bool viewports_need_updating_ = true;
  gvr::SwapChain swap_chain_;
  gvr::Frame acquired_frame_;
  WebVrBounds current_webvr_frame_bounds_ =
      WebVrBounds(gfx::RectF(), gfx::RectF(), gfx::Size());

  // The default size for the render buffers.
  gfx::Size render_size_default_;
  gfx::Size render_size_webvr_ui_;
  const bool low_density_;

  bool webxr_use_shared_buffer_draw_ = false;

  gfx::Size content_tex_buffer_size_ = {0, 0};
  gfx::Size webxr_surface_size_ = {0, 0};

  const bool surfaceless_rendering_;
  bool content_paused_;

  GlBrowserInterface* browser_;

  // This callback should be called once a GL context is active and textures
  // have been created.
  TexturesInitializedCallback textures_initialized_callback_;

  // GVR acquire/submit times for scheduling heuristics.
  device::SlidingTimeDeltaAverage webvr_acquire_time_;
  device::SlidingTimeDeltaAverage webvr_submit_time_;

  gfx::Point3F pointer_start_;

  RenderInfo render_info_;

  std::vector<gvr::BufferSpec> specs_;

  std::string frame_buffer_dump_filepath_base_;
  std::string frame_buffer_dump_filepath_suffix_;
  unsigned int last_bound_buffer_index_;

  base::WeakPtrFactory<GvrGraphicsDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GvrGraphicsDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_GVR_GRAPHICS_DELEGATE_H_
