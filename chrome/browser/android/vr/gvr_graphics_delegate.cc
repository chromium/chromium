// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/gvr_graphics_delegate.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "chrome/browser/android/vr/gl_browser_interface.h"
#include "chrome/browser/android/vr/gvr_util.h"
#include "chrome/browser/vr/vr_geometry_util.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/vr_gl_util.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace vr {

namespace {
// GVR buffer indices for use with viewport->SetSourceBufferIndex
// or frame.BindBuffer. We use one for multisampled contents (Browser UI), and
// one for non-multisampled content (webVR or quad layer).
constexpr int kMultiSampleBuffer = 0;
constexpr int kNoMultiSampleBuffer = 1;

constexpr float kDefaultRenderTargetSizeScale = 0.75f;

// When display UI on top of WebVR, we use a separate buffer. Normally, the
// buffer is set to recommended size to get best visual (i.e the buffer for
// rendering ChromeVR). We divide the recommended buffer size by this number to
// improve performance.
// We calculate a smaller FOV and UV per frame which includes all visible
// elements. This allows us rendering UI at the same quality with a smaller
// buffer.
// Use 2 for now, we can probably make the buffer even smaller.
constexpr float kWebVrBrowserUiSizeFactor = 2.f;

// If we're not using the SurfaceTexture, use this matrix instead of
// webvr_surface_texture_uv_transform_ for drawing to GVR.
constexpr float kWebVrIdentityUvTransform[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                                 0, 0, 1, 0, 0, 0, 0, 1};

gfx::Transform PerspectiveMatrixFromView(const gvr::Rectf& fov,
                                         float z_near,
                                         float z_far) {
  gfx::Transform result;
  const float x_left = -std::tan(gfx::DegToRad(fov.left)) * z_near;
  const float x_right = std::tan(gfx::DegToRad(fov.right)) * z_near;
  const float y_bottom = -std::tan(gfx::DegToRad(fov.bottom)) * z_near;
  const float y_top = std::tan(gfx::DegToRad(fov.top)) * z_near;

  DCHECK(x_left < x_right && y_bottom < y_top && z_near < z_far &&
         z_near > 0.0f && z_far > 0.0f);
  const float X = (2 * z_near) / (x_right - x_left);
  const float Y = (2 * z_near) / (y_top - y_bottom);
  const float A = (x_right + x_left) / (x_right - x_left);
  const float B = (y_top + y_bottom) / (y_top - y_bottom);
  const float C = (z_near + z_far) / (z_near - z_far);
  const float D = (2 * z_near * z_far) / (z_near - z_far);

  // The gfx::Transform default ctor initializes the transform to the identity,
  // so we must zero out a few values along the diagonal here.
  result.set_rc(0, 0, X);
  result.set_rc(0, 2, A);
  result.set_rc(1, 1, Y);
  result.set_rc(1, 2, B);
  result.set_rc(2, 2, C);
  result.set_rc(2, 3, D);
  result.set_rc(3, 2, -1);
  result.set_rc(3, 3, 0);

  return result;
}

gvr::Rectf UVFromGfxRect(gfx::RectF rect) {
  return {rect.x(), rect.x() + rect.width(), 1.0f - rect.bottom(),
          1.0f - rect.y()};
}

gfx::RectF GfxRectFromUV(gvr::Rectf rect) {
  return gfx::RectF(rect.left, 1.0 - rect.top, rect.right - rect.left,
                    rect.top - rect.bottom);
}

gfx::RectF ClampRect(gfx::RectF bounds) {
  bounds.AdjustToFit(gfx::RectF(0, 0, 1, 1));
  return bounds;
}

gvr::Rectf ToGvrRectf(const FovRectangle& rect) {
  return gvr::Rectf{rect.left, rect.right, rect.bottom, rect.top};
}

FovRectangle ToUiFovRect(const gvr::Rectf& rect) {
  return FovRectangle{rect.left, rect.right, rect.bottom, rect.top};
}

}  // namespace

GvrGraphicsDelegate::GvrGraphicsDelegate(
    GlBrowserInterface* browser,
    base::OnceClosure gl_initialized_callback,
    gvr::GvrApi* gvr_api,
    bool reprojected_rendering,
    size_t sliding_time_size)
    : gvr_api_(gvr_api),
      surfaceless_rendering_(reprojected_rendering),
      browser_(browser),
      gl_initialized_callback_(std::move(gl_initialized_callback)),
      webvr_acquire_time_(sliding_time_size),
      webvr_submit_time_(sliding_time_size) {}

GvrGraphicsDelegate::~GvrGraphicsDelegate() {
  if (curr_context_id_ != kNone) {
    contexts_[curr_context_id_]->ReleaseCurrent(surface_.get());
  }
}

void GvrGraphicsDelegate::Init(
    base::WaitableEvent* gl_surface_created_event,
    base::OnceCallback<gfx::AcceleratedWidget()> callback) {
  if (surfaceless_rendering_) {
    InitializeGl(nullptr);
  } else {
    gl_surface_created_event->Wait();
    InitializeGl(std::move(callback).Run());
  }
}

void GvrGraphicsDelegate::InitializeGl(gfx::AcceleratedWidget window) {
  // We can only share native GL resources with GVR, and GVR doesn't support
  // ANGLE, so disable it.
  // TODO(crbug.com/1170580): support ANGLE with cardboard?
  gl::init::DisableANGLE();

  gl::GLDisplay* display = nullptr;
  if (gl::GetGLImplementation() == gl::kGLImplementationNone) {
    display = gl::init::InitializeGLOneOff(
        /*gpu_preference=*/gl::GpuPreference::kDefault);
    if (!display) {
      LOG(ERROR) << "gl::init::InitializeGLOneOff failed";
      browser_->ForceExitVr();
      return;
    }
  } else {
    display = gl::GetDefaultDisplayEGL();
  }

  scoped_refptr<gl::GLSurface> surface;
  if (window) {
    DCHECK(!surfaceless_rendering_);
    surface = gl::init::CreateViewGLSurface(display, window);
  } else {
    DCHECK(surfaceless_rendering_);
    surface = gl::init::CreateOffscreenGLSurface(display, gfx::Size());
  }
  if (!surface.get()) {
    LOG(ERROR) << "gl::init::CreateOffscreenGLSurface failed";
    browser_->ForceExitVr();
    return;
  }

  surface_ = surface;
  share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
  for (auto& context : contexts_) {
    context = gl::init::CreateGLContext(share_group_.get(), surface_.get(),
                                        gl::GLContextAttribs());
    if (!context.get()) {
      LOG(ERROR) << "gl::init::CreateGLContext failed";
      browser_->ForceExitVr();
      return;
    }
  }

  if (!MakeContextCurrent(kMainContext)) {
    browser_->ForceExitVr();
    return;
  }

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  unsigned int textures[1];
  glGenTextures(1, textures);
  webvr_texture_id_ = textures[0];

  // InitializeRenderer calls GvrDelegateReady which triggers actions such as
  // responding to RequestPresent.
  InitializeRenderer();

  std::move(gl_initialized_callback_).Run();
}

bool GvrGraphicsDelegate::CreateOrResizeWebXrSurface(
    const gfx::Size& size,
    base::RepeatingClosure on_webxr_frame_available) {
  DVLOG(2) << __func__ << ": size=" << size.width() << "x" << size.height();
  if (webxr_use_shared_buffer_draw_) {
    DCHECK(!webxr_surface_texture_);
    // We don't have a surface in SharedBuffer mode, just update the size.
    webxr_surface_size_ = size;
    return true;
  }
  if (!webxr_surface_texture_) {
    DCHECK(on_webxr_frame_available)
        << "A callback must be provided to create the surface texture";
    webxr_surface_texture_ = gl::SurfaceTexture::Create(webvr_texture_id_);
    webxr_surface_texture_->SetFrameAvailableCallback(
        std::move(on_webxr_frame_available));
  }

  // ContentPhysicalBoundsChanged is getting called twice with
  // identical sizes? Avoid thrashing the existing context.
  if (webxr_->mailbox_bridge_ready() && size == webxr_surface_size_) {
    DVLOG(1) << "Ignore resize, size is unchanged";
    return false;
  }

  if (size.IsEmpty()) {
    // Defer until a new size arrives on a future bounds update.
    DVLOG(1) << "Ignore resize, invalid size";
    return false;
  }

  webxr_surface_texture_->SetDefaultBufferSize(size.width(), size.height());
  webxr_surface_size_ = size;
  return true;
}

base::TimeDelta GvrGraphicsDelegate::GetAcquireTimeAverage() const {
  return webvr_acquire_time_.GetAverage();
}

void GvrGraphicsDelegate::OnWebXrFrameAvailable() {
  webxr_surface_texture_->UpdateTexImage();
  // The usual transform matrix we get for the Surface flips Y, so we need to
  // apply it in the copy shader to get the correct image orientation:
  //  {1,  0, 0, 0,
  //   0, -1, 0, 0,
  //   0,  0, 1, 0,
  //   0,  1, 0, 1}
  webxr_surface_texture_->GetTransformMatrix(
      &webvr_surface_texture_uv_transform_[0]);
}

void GvrGraphicsDelegate::InitializeRenderer() {
  gvr_api_->InitializeGl();

  std::vector<gvr::BufferSpec> specs;

  // Create multisampled and non-multisampled buffers.
  specs.push_back(gvr_api_->CreateBufferSpec());
  specs.push_back(gvr_api_->CreateBufferSpec());

  gvr::Sizei max_size = gvr_api_->GetMaximumEffectiveRenderTargetSize();
  float scale = kDefaultRenderTargetSizeScale;

  render_size_default_ = {base::ClampRound(max_size.width * scale),
                          base::ClampRound(max_size.height * scale)};
  render_size_webvr_ui_ = {
      base::ClampRound(max_size.width / kWebVrBrowserUiSizeFactor),
      base::ClampRound(max_size.height / kWebVrBrowserUiSizeFactor)};

  specs[kMultiSampleBuffer].SetSamples(2);
  specs[kMultiSampleBuffer].SetDepthStencilFormat(
      GVR_DEPTH_STENCIL_FORMAT_NONE);
  specs[kMultiSampleBuffer].SetSize(render_size_webvr_ui_.width(),
                                    render_size_webvr_ui_.height());

  specs[kNoMultiSampleBuffer].SetSamples(1);
  specs[kNoMultiSampleBuffer].SetDepthStencilFormat(
      GVR_DEPTH_STENCIL_FORMAT_NONE);
  specs[kNoMultiSampleBuffer].SetSize(render_size_default_.width(),
                                      render_size_default_.height());

  swap_chain_ = gvr_api_->CreateSwapChain(specs);

  UpdateViewports();

  browser_->GvrDelegateReady();
}

void GvrGraphicsDelegate::UpdateViewports() {
  if (!viewports_need_updating_)
    return;
  viewports_need_updating_ = false;

  // Set up main viewports. The list has two elements, 0=left eye and
  // 1=right eye.
  gvr::BufferViewportList viewport_list =
      gvr_api_->CreateEmptyBufferViewportList();

  viewport_list.SetToRecommendedBufferViewports();
  // Save copies of the first two viewport items for use by WebVR, it sets its
  // own UV bounds.
  viewport_list.GetBufferViewport(0, &webvr_viewport_.left);
  viewport_list.GetBufferViewport(1, &webvr_viewport_.right);
  webvr_viewport_.left.SetSourceUv(
      UVFromGfxRect(ClampRect(current_webvr_frame_bounds_.left_bounds)));
  webvr_viewport_.right.SetSourceUv(
      UVFromGfxRect(ClampRect(current_webvr_frame_bounds_.right_bounds)));

  viewport_list.GetBufferViewport(0, &webvr_overlay_viewport_.left);
  viewport_list.GetBufferViewport(1, &webvr_overlay_viewport_.right);

  webvr_overlay_viewport_.SetSourceBufferIndex(kMultiSampleBuffer);
  webvr_viewport_.SetSourceBufferIndex(kNoMultiSampleBuffer);
}

void GvrGraphicsDelegate::SetWebXrBounds(const WebVrBounds& bounds) {
  webvr_viewport_.left.SetSourceUv(UVFromGfxRect(bounds.left_bounds));
  webvr_viewport_.right.SetSourceUv(UVFromGfxRect(bounds.right_bounds));
  current_webvr_frame_bounds_ =
      bounds;  // If we recreate the viewports, keep these bounds.
}

bool GvrGraphicsDelegate::ResizeForWebXr() {
  // Resize the webvr overlay buffer, which may have been used by the content
  // quad buffer previously.
  gvr::Sizei size = swap_chain_.GetBufferSize(kMultiSampleBuffer);
  gfx::Size target_size = render_size_webvr_ui_;
  if (size.width != target_size.width() ||
      size.height != target_size.height()) {
    swap_chain_.ResizeBuffer(kMultiSampleBuffer,
                             {target_size.width(), target_size.height()});
  }

  size = swap_chain_.GetBufferSize(kNoMultiSampleBuffer);
  target_size = webxr_surface_size_;
  if (!target_size.width()) {
    // Don't try to resize to 0x0 pixels, drop frames until we get a valid
    // size.
    return false;
  }
  if (size.width != target_size.width() ||
      size.height != target_size.height()) {
    swap_chain_.ResizeBuffer(kNoMultiSampleBuffer,
                             {target_size.width(), target_size.height()});
  }
  return true;
}

void GvrGraphicsDelegate::ResizeForBrowser() {
  gvr::Sizei size = swap_chain_.GetBufferSize(kMultiSampleBuffer);
  gfx::Size target_size = render_size_default_;
  if (size.width != target_size.width() ||
      size.height != target_size.height()) {
    swap_chain_.ResizeBuffer(kMultiSampleBuffer,
                             {target_size.width(), target_size.height()});
  }
}

void GvrGraphicsDelegate::UpdateEyeInfos(const gfx::Transform& head_pose,
                                         const Viewport& viewport,
                                         const gfx::Size& render_size,
                                         RenderInfo* out_render_info) {
  for (auto eye : {GVR_LEFT_EYE, GVR_RIGHT_EYE}) {
    CameraModel& eye_info = (eye == GVR_LEFT_EYE)
                                ? out_render_info->left_eye_model
                                : out_render_info->right_eye_model;
    eye_info.eye_type =
        (eye == GVR_LEFT_EYE) ? EyeType::kLeftEye : EyeType::kRightEye;

    const gvr::BufferViewport& vp =
        (eye == GVR_LEFT_EYE) ? viewport.left : viewport.right;

    gfx::Transform eye_matrix;
    GvrMatToTransform(gvr_api_->GetEyeFromHeadMatrix(eye), &eye_matrix);
    eye_info.view_matrix = eye_matrix * head_pose;

    const gfx::RectF& rect = GfxRectFromUV(vp.GetSourceUv());
    eye_info.viewport = vr::CalculatePixelSpaceRect(render_size, rect);

    eye_info.view_proj_matrix =
        PerspectiveMatrixFromView(vp.GetSourceFov(), GetZNear(), GetZFar()) *
        eye_info.view_matrix;
  }
}

bool GvrGraphicsDelegate::AcquireGvrFrame(int frame_index) {
  TRACE_EVENT0("gpu", "Vr.AcquireGvrFrame");
  DCHECK(!acquired_frame_);
  base::TimeTicks acquire_start = base::TimeTicks::Now();
  acquired_frame_ = swap_chain_.AcquireFrame();
  webvr_acquire_time_.AddSample(base::TimeTicks::Now() - acquire_start);
  return !!acquired_frame_;
}

RenderInfo GvrGraphicsDelegate::GetRenderInfo(FrameType frame_type,
                                              const gfx::Transform& head_pose) {
  gfx::Size render_size =
      frame_type == kWebXrFrame ? render_size_webvr_ui_ : render_size_default_;
  UpdateEyeInfos(head_pose, webvr_overlay_viewport_, render_size,
                 &render_info_);
  render_info_.head_pose = head_pose;
  return render_info_;
}

void GvrGraphicsDelegate::InitializeBuffers() {
  viewport_list_ = gvr_api_->CreateEmptyBufferViewportList();
}

void GvrGraphicsDelegate::PrepareBufferForWebXr() {
  DCHECK_EQ(viewport_list_.GetSize(), 0U);
  viewport_list_.SetBufferViewport(0, webvr_viewport_.left);
  viewport_list_.SetBufferViewport(1, webvr_viewport_.right);
  acquired_frame_.BindBuffer(kNoMultiSampleBuffer);
  if (webxr_use_shared_buffer_draw_) {
    WebVrWaitForServerFence();
    CHECK(webxr_->HaveProcessingFrame());
  }
  // We're redrawing over the entire viewport, but it's generally more
  // efficient on mobile tiling GPUs to clear anyway as a hint that
  // we're done with the old content. TODO(https://crbug.com/700389):
  // investigate using glDiscardFramebufferEXT here since that's more
  // efficient on desktop, but it would need a capability check since
  // it's not supported on older devices such as Nexus 5X.
  glClear(GL_COLOR_BUFFER_BIT);
  // Don't need face culling, depth testing, blending, etc. Turn it all off.
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_POLYGON_OFFSET_FILL);

  glViewport(0, 0, webxr_surface_size_.width(), webxr_surface_size_.height());
}

void GvrGraphicsDelegate::PrepareBufferForWebXrOverlayElements() {
  // WebVR content may use an arbitrary size buffer. We need to draw browser
  // UI on a different buffer to make sure that our UI has enough resolution.
  acquired_frame_.BindBuffer(kMultiSampleBuffer);
  DCHECK_EQ(viewport_list_.GetSize(), 2U);
  viewport_list_.SetBufferViewport(2, webvr_overlay_viewport_.left);
  viewport_list_.SetBufferViewport(3, webvr_overlay_viewport_.right);
  glClear(GL_COLOR_BUFFER_BIT);
}

void GvrGraphicsDelegate::PrepareBufferForBrowserUi() {
  DCHECK_LE(viewport_list_.GetSize(), 2U);
  viewport_list_.SetBufferViewport(viewport_list_.GetSize(),
                                   webvr_overlay_viewport_.left);
  viewport_list_.SetBufferViewport(viewport_list_.GetSize(),
                                   webvr_overlay_viewport_.right);
  acquired_frame_.BindBuffer(kMultiSampleBuffer);
  glClear(GL_COLOR_BUFFER_BIT);
}

FovRectangles GvrGraphicsDelegate::GetRecommendedFovs() {
  return {ToUiFovRect(webvr_overlay_viewport_.left.GetSourceFov()),
          ToUiFovRect(webvr_overlay_viewport_.right.GetSourceFov())};
}

RenderInfo GvrGraphicsDelegate::GetOptimizedRenderInfoForFovs(
    const FovRectangles& fovs) {
  webvr_overlay_viewport_.left.SetSourceFov(ToGvrRectf(fovs.first));
  webvr_overlay_viewport_.right.SetSourceFov(ToGvrRectf(fovs.second));

  // Set render info to recommended setting. It will be used as our base for
  // optimization.
  RenderInfo render_info_webxr_overlay;
  render_info_webxr_overlay.head_pose = render_info_.head_pose;
  UpdateEyeInfos(render_info_webxr_overlay.head_pose, webvr_overlay_viewport_,
                 render_size_webvr_ui_, &render_info_webxr_overlay);
  return render_info_webxr_overlay;
}

void GvrGraphicsDelegate::OnFinishedDrawingBuffer() {
  acquired_frame_.Unbind();
}

void GvrGraphicsDelegate::GetWebXrDrawParams(int* texture_id,
                                             Transform* uv_transform) {
  if (webxr_use_shared_buffer_draw_) {
    device::WebXrSharedBuffer* buffer =
        webxr_->GetProcessingFrame()->shared_buffer.get();
    CHECK(buffer);
    *texture_id = buffer->local_texture;
    // Use an identity UV transform, the image is already oriented correctly.
    base::ranges::copy(kWebVrIdentityUvTransform, *uv_transform);
  } else {
    *texture_id = webvr_texture_id_;
    // Apply the UV transform from the SurfaceTexture, that's usually a Y flip.
    base::ranges::copy(webvr_surface_texture_uv_transform_, *uv_transform);
  }
}

void GvrGraphicsDelegate::WebVrWaitForServerFence() {
  DCHECK(webxr_->HaveProcessingFrame());

  std::unique_ptr<gl::GLFence> gpu_fence(
      webxr_->GetProcessingFrame()->gvr_handoff_fence.release());

  DCHECK(gpu_fence);
  // IMPORTANT: wait as late as possible to insert the server wait. Doing so
  // blocks the server from doing any further work on this GL context until
  // the fence signals, this prevents any older fences such as the ones we
  // may be using for other synchronization from signaling.

  gpu_fence->ServerWait();
  // Fence will be destroyed on going out of scope here.
  return;
}

void GvrGraphicsDelegate::SubmitToGvr(const gfx::Transform& head_pose) {
  TRACE_EVENT0("gpu", __func__);
  gvr::Mat4f mat;
  TransformToGvrMat(head_pose, &mat);
  base::TimeTicks submit_start = base::TimeTicks::Now();
  acquired_frame_.Submit(viewport_list_, mat);
  base::TimeTicks submit_done = base::TimeTicks::Now();
  webvr_submit_time_.AddSample(submit_done - submit_start);
  CHECK(!acquired_frame_);
  // No need to swap buffers for surfaceless rendering.
  if (!surfaceless_rendering_) {
    // TODO(mthiesse): Support asynchronous SwapBuffers.
    SwapSurfaceBuffers();
  }
}

base::WeakPtr<GvrGraphicsDelegate> GvrGraphicsDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool GvrGraphicsDelegate::DoesSurfacelessRendering() const {
  return surfaceless_rendering_;
}

void GvrGraphicsDelegate::RecordFrameTimeTraces() const {
  TRACE_COUNTER2("gpu", "GVR frame time (ms)", "acquire",
                 webvr_acquire_time_.GetAverage().InMilliseconds(), "submit",
                 webvr_submit_time_.GetAverage().InMilliseconds());
}

bool GvrGraphicsDelegate::RunInSkiaContext(base::OnceClosure callback) {
  DCHECK_EQ(curr_context_id_, kMainContext);
  if (!MakeContextCurrent(kSkiaContext)) {
    return false;
  }
  std::move(callback).Run();
  return MakeContextCurrent(kMainContext);
}

void GvrGraphicsDelegate::SwapSurfaceBuffers() {
  TRACE_EVENT0("gpu", __func__);
  DCHECK(surface_);
  surface_->SwapBuffers(base::DoNothing(), gfx::FrameData());
}

bool GvrGraphicsDelegate::MakeContextCurrent(ContextId context_id) {
  DCHECK(context_id > kNone && context_id < kNumContexts);
  if (curr_context_id_ == context_id) {
    return true;
  }
  auto& context = contexts_[context_id];
  if (!context->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return false;
  }
  curr_context_id_ = context_id;
  return true;
}

bool GvrGraphicsDelegate::PreRender() {
  // Only called by VrBrowserRendererThreadWin which never creates this class.
  NOTREACHED_NORETURN();
}

void GvrGraphicsDelegate::PostRender() {
  // Only called by VrBrowserRendererThreadWin which never creates this class.
  NOTREACHED_NORETURN();
}

mojo::PlatformHandle GvrGraphicsDelegate::GetTexture() {
  // Only called by VrBrowserRendererThreadWin which never creates this class.
  NOTREACHED_NORETURN();
}

const gpu::SyncToken& GvrGraphicsDelegate::GetSyncToken() {
  // Only called by VrBrowserRendererThreadWin which never creates this class.
  NOTREACHED_NORETURN();
}

void GvrGraphicsDelegate::ResetMemoryBuffer() {
  // Only called by VrBrowserRendererThreadWin which never creates this class.
  NOTREACHED_NORETURN();
}

bool GvrGraphicsDelegate::BindContext() {
  // Only called by VrBrowserRendererThreadWin which never creates this class.
  NOTREACHED_NORETURN();
}

void GvrGraphicsDelegate::ClearContext() {
  // Only called by VrBrowserRendererThreadWin which never creates this class.
  NOTREACHED_NORETURN();
}
}  // namespace vr
