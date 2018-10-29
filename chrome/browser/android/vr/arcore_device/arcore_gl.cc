// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_gl.h"

#include <algorithm>
#include <limits>
#include <utility>
#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/jni_android.h"
#include "base/callback_helpers.h"
#include "base/containers/queue.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/traced_value.h"
#include "chrome/browser/android/vr/arcore_device/ar_image_transport.h"
#include "chrome/browser/android/vr/arcore_device/arcore_impl.h"
#include "chrome/browser/android/vr/arcore_device/arcore_install_utils.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace {
// Input display coordinates (range 0..1) used with ArCore's
// transformDisplayUvCoords to calculate the output matrix.
constexpr std::array<float, 6> kDisplayCoordinatesForTransform = {
    0.f, 0.f, 1.f, 0.f, 0.f, 1.f};

gfx::Transform ConvertUvsToTransformMatrix(const std::vector<float>& uvs) {
  // We're creating a matrix that transforms viewport UV coordinates (for a
  // screen-filling quad, origin at bottom left, u=1 at right, v=1 at top) to
  // camera texture UV coordinates. This matrix is used to compute texture
  // coordinates for copying an appropriately cropped and rotated subsection of
  // the camera image. The SampleData is a bit unfortunate. ArCore doesn't
  // provide a way to get a matrix directly. There's a function to transform UV
  // vectors individually, which obviously can't be used from a shader, so we
  // run that on selected vectors and recreate the matrix from the result.

  // Assumes that |uvs| is the result of transforming the display coordinates
  // from kDisplayCoordinatesForTransform. This combines the solved matrix with
  // a Y flip because ArCore's "normalized screen space" coordinates have the
  // origin at the top left to match 2D Android APIs, so it needs a Y flip to
  // get an origin at bottom left as used for textures.
  DCHECK_EQ(uvs.size(), 6U);
  float u00 = uvs[0];
  float v00 = uvs[1];
  float u10 = uvs[2];
  float v10 = uvs[3];
  float u01 = uvs[4];
  float v01 = uvs[5];

  // Transform initializes to the identity matrix and then is modified by uvs.
  gfx::Transform result;
  result.matrix().set(0, 0, u10 - u00);
  result.matrix().set(0, 1, -(u01 - u00));
  result.matrix().set(0, 3, u01);
  result.matrix().set(1, 0, v10 - v00);
  result.matrix().set(1, 1, -(v01 - v00));
  result.matrix().set(1, 3, v01);
  return result;
}

}  // namespace

namespace device {

struct ArCoreHitTestRequest {
  ArCoreHitTestRequest() = default;
  ~ArCoreHitTestRequest() = default;
  mojom::XRRayPtr ray;
  mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback callback;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArCoreHitTestRequest);
};

ArCoreGl::ArCoreGl(std::unique_ptr<ArImageTransport> ar_image_transport)
    : gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      ar_image_transport_(std::move(ar_image_transport)),
      weak_ptr_factory_(this) {}

ArCoreGl::~ArCoreGl() {}

void ArCoreGl::Initialize(vr::ArCoreInstallUtils* install_utils,
                          ArCoreFactory* arcore_factory,
                          base::OnceCallback<void(bool)> callback) {
  DCHECK(IsOnGlThread());

  // Do not DCHECK !is_initialized to allow multiple calls to correctly
  // proceed. This method may be called multiple times if a subsequent session
  // request occurs before the first one completes and the callback is called.
  // TODO(https://crbug.com/849568): This may not be necessary after
  // addressing this issue.
  if (is_initialized_) {
    std::move(callback).Run(true);
    return;
  }

  if (!InitializeGl()) {
    std::move(callback).Run(false);
    return;
  }

  // Get the activity context.
  base::android::ScopedJavaLocalRef<jobject> application_context =
      install_utils->GetApplicationContext();
  if (!application_context.obj()) {
    DLOG(ERROR) << "Unable to retrieve the Java context/activity!";
    std::move(callback).Run(false);
    return;
  }

  arcore_ = arcore_factory->Create();
  if (!arcore_->Initialize(application_context)) {
    DLOG(ERROR) << "ARCore failed to initialize";
    std::move(callback).Run(false);
    return;
  }

  // Set the texture on ArCore to render the camera.
  arcore_->SetCameraTexture(ar_image_transport_->GetCameraTextureId());
  // Set the Geometry to ensure consistent behaviour.
  arcore_->SetDisplayGeometry(gfx::Size(0, 0), display::Display::ROTATE_0);

  is_initialized_ = true;

  std::move(callback).Run(true);
}

bool ArCoreGl::InitializeGl() {
  DCHECK(IsOnGlThread());
  DCHECK(!is_initialized_);

  if (gl::GetGLImplementation() == gl::kGLImplementationNone &&
      !gl::init::InitializeGLOneOff()) {
    DLOG(ERROR) << "gl::init::InitializeGLOneOff failed";
    return false;
  }

  scoped_refptr<gl::GLSurface> surface =
      gl::init::CreateOffscreenGLSurface(gfx::Size());
  if (!surface.get()) {
    DLOG(ERROR) << "gl::init::CreateOffscreenGLSurface failed";
    return false;
  }

  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  if (!context.get()) {
    DLOG(ERROR) << "gl::init::CreateGLContext failed";
    return false;
  }
  if (!context->MakeCurrent(surface.get())) {
    DLOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return false;
  }

  if (!ar_image_transport_->Initialize()) {
    DLOG(ERROR) << "ARImageTransport failed to initialize";
    return false;
  }

  // Assign the surface and context members now that initialization has
  // succeeded.
  surface_ = std::move(surface);
  context_ = std::move(context);

  return true;
}

void ArCoreGl::ProduceFrame(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  // Check if the frame_size and display_rotation updated last frame. If yes,
  // apply the update for this frame.
  if (should_recalculate_uvs_) {
    // Get the UV transform matrix from ArCore's UV transform.
    std::vector<float> uvs_transformed =
        arcore_->TransformDisplayUvCoords(kDisplayCoordinatesForTransform);
    uv_transform_ = ConvertUvsToTransformMatrix(uvs_transformed);

    // We need near/far distances to make a projection matrix. The actual
    // values don't matter, the Renderer will recalculate dependent values
    // based on the application's near/far settngs.
    constexpr float depth_near = 0.1f;
    constexpr float depth_far = 1000.f;
    projection_ = arcore_->GetProjectionMatrix(depth_near, depth_far);
    should_recalculate_uvs_ = false;
  }

  // Now check if the frame_size or display_rotation neds to be updated
  // for the next frame. This must happen after the should_recalculate_uvs_
  // check above to ensure it executes with the needed one-frame delay.
  if (transfer_size_ != frame_size || display_rotation_ != display_rotation) {
    // Set display geometry before calling Update. It's a pending request that
    // applies to the next frame.
    arcore_->SetDisplayGeometry(frame_size, display_rotation);

    // Store the passed in values to ensure that we can update them only if they
    // change.
    transfer_size_ = frame_size;
    display_rotation_ = display_rotation;

    // Tell the uvs to recalculate on the next animation frame, by which time
    // SetDisplayGeometry will have set the new values in arcore_.
    should_recalculate_uvs_ = true;
  }

  TRACE_EVENT_BEGIN0("gpu", "ArCore Update");
  bool camera_updated = false;
  mojom::VRPosePtr pose = arcore_->Update(&camera_updated);
  TRACE_EVENT_END0("gpu", "ArCore Update");
  if (!camera_updated) {
    DVLOG(1) << "arcore_->Update() failed";
    std::move(callback).Run(nullptr);
    return;
  }

  // Transfer the camera image texture to a MailboxHolder for transport to
  // the renderer process.
  gpu::MailboxHolder buffer_holder =
      ar_image_transport_->TransferFrame(transfer_size_, uv_transform_);

  // Create the frame data to return to the renderer.
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();
  frame_data->pose = std::move(pose);
  frame_data->buffer_holder = buffer_holder;
  frame_data->buffer_size = transfer_size_;
  frame_data->time_delta = base::TimeTicks::Now() - base::TimeTicks();
  // Convert the Transform's 4x4 matrix to 16 floats in column-major order.
  frame_data->projection_matrix.emplace(16);
  projection_.matrix().asColMajorf(frame_data->projection_matrix->data());

  fps_meter_.AddFrame(base::TimeTicks::Now());
  TRACE_COUNTER1("gpu", "WebXR FPS", fps_meter_.GetFPS());

  // Post a task to finish processing the frame so any calls to
  // RequestHitTest() that were made during this function, which can block
  // on the arcore_->Update() call above, can be processed in this frame.
  gl_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ArCoreGl::ProcessFrame, weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&frame_data), frame_size,
                     base::Passed(&callback)));
}

void ArCoreGl::RequestHitTest(
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback callback) {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  std::unique_ptr<ArCoreHitTestRequest> request =
      std::make_unique<ArCoreHitTestRequest>();
  request->ray = std::move(ray);
  request->callback = std::move(callback);
  hit_test_requests_.push_back(std::move(request));
}

void ArCoreGl::ProcessFrame(
    mojom::XRFrameDataPtr frame_data,
    const gfx::Size& frame_size,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  // The timing requirements for hit-test are documented here:
  // https://github.com/immersive-web/hit-test/blob/master/explainer.md#timing
  // The current implementation of frame generation on the renderer side is
  // 1:1 with calls to this method, so it is safe to fire off the hit-test
  // results here, one at a time, in the order they were enqueued prior to
  // running the GetFrameDataCallback.
  // Since mojo callbacks are processed in order, this will result in the
  // correct sequence of hit-test callbacks / promise resolutions. If
  // the implementation of the renderer processing were to change, this
  // code is fragile and could break depending on the new implementation.
  // TODO(https://crbug.com/844174): In order to be more correct by design,
  // hit results should be bundled with the frame data - that way it would be
  // obvious how the timing between the results and the frame should go.
  for (auto& request : hit_test_requests_) {
    std::vector<mojom::XRHitResultPtr> results;
    if (arcore_->RequestHitTest(request->ray, frame_size, &results)) {
      std::move(request->callback).Run(std::move(results));
    } else {
      // Hit test failed, i.e. unprojected location was offscreen.
      std::move(request->callback).Run(base::nullopt);
    }
  }
  hit_test_requests_.clear();

  // Running this callback after resolving all the hit-test requests ensures
  // that we satisfy the guarantee of the WebXR hit-test spec - that the
  // hit-test promise resolves immediately prior to the frame for which it is
  // valid.
  std::move(callback).Run(std::move(frame_data));
}

void ArCoreGl::Pause() {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  arcore_->Pause();
}

void ArCoreGl::Resume() {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  arcore_->Resume();
}

bool ArCoreGl::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

base::WeakPtr<ArCoreGl> ArCoreGl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace device
