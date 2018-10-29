// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_impl.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/numerics/math_constants.h"
#include "base/optional.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/vr/arcore_device/arcore_java_utils.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"

using base::android::JavaRef;

namespace device {

ArCoreImpl::ArCoreImpl()
    : gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {}

ArCoreImpl::~ArCoreImpl() = default;

bool ArCoreImpl::Initialize(
    base::android::ScopedJavaLocalRef<jobject> context) {
  DCHECK(IsOnGlThread());
  DCHECK(!arcore_session_.is_valid());

  // TODO(https://crbug.com/837944): Notify error earlier if this will fail.

  JNIEnv* env = base::android::AttachCurrentThread();
  if (!env) {
    DLOG(ERROR) << "Unable to get JNIEnv for ArCore";
    return false;
  }

  // Use a local scoped ArSession for the next steps, we want the
  // arcore_session_ member to remain null until we complete successful
  // initialization.
  internal::ScopedArCoreObject<ArSession*> session;

  ArStatus status = ArSession_create(env, context.obj(), session.receive());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_create failed: " << status;
    return false;
  }

  internal::ScopedArCoreObject<ArConfig*> arcore_config;
  ArConfig_create(session.get(), arcore_config.receive());
  if (!arcore_config.is_valid()) {
    DLOG(ERROR) << "ArConfig_create failed";
    return false;
  }

  // We just use the default config.
  status = ArSession_checkSupported(session.get(), arcore_config.get());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_checkSupported failed: " << status;
    return false;
  }

  status = ArSession_configure(session.get(), arcore_config.get());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_configure failed: " << status;
    return false;
  }

  internal::ScopedArCoreObject<ArFrame*> frame;

  ArFrame_create(session.get(), frame.receive());
  if (!frame.is_valid()) {
    DLOG(ERROR) << "ArFrame_create failed";
    return false;
  }

  // Success, we now have a valid session and a valid frame.
  arcore_frame_ = std::move(frame);
  arcore_session_ = std::move(session);
  return true;
}

void ArCoreImpl::SetCameraTexture(GLuint camera_texture_id) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  ArSession_setCameraTextureName(arcore_session_.get(), camera_texture_id);
}

void ArCoreImpl::SetDisplayGeometry(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  // Display::Rotation is the same as Android's rotation and is compatible with
  // what ArCore is expecting.
  ArSession_setDisplayGeometry(arcore_session_.get(), display_rotation,
                               frame_size.width(), frame_size.height());
}

std::vector<float> ArCoreImpl::TransformDisplayUvCoords(
    const base::span<const float> uvs) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());

  size_t num_elements = uvs.size();
  DCHECK(num_elements % 2 == 0);
  std::vector<float> uvs_out(num_elements);
  ArFrame_transformDisplayUvCoords(arcore_session_.get(), arcore_frame_.get(),
                                   num_elements, &uvs[0], &uvs_out[0]);
  return uvs_out;
}

mojom::VRPosePtr ArCoreImpl::Update(bool* camera_updated) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());
  DCHECK(camera_updated);

  ArStatus status;

  status = ArSession_update(arcore_session_.get(), arcore_frame_.get());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_update failed: " << status;
    *camera_updated = false;
    return nullptr;
  }

  // If we get here, assume we have a valid camera image, but we don't know yet
  // if tracking is working.
  *camera_updated = true;
  internal::ScopedArCoreObject<ArCamera*> arcore_camera;
  ArFrame_acquireCamera(arcore_session_.get(), arcore_frame_.get(),
                        arcore_camera.receive());
  if (!arcore_camera.is_valid()) {
    DLOG(ERROR) << "ArFrame_acquireCamera failed!";
    return nullptr;
  }

  ArTrackingState tracking_state;
  ArCamera_getTrackingState(arcore_session_.get(), arcore_camera.get(),
                            &tracking_state);
  if (tracking_state != AR_TRACKING_STATE_TRACKING) {
    DVLOG(1) << "Tracking state is not AR_TRACKING_STATE_TRACKING: "
             << tracking_state;
    return nullptr;
  }

  internal::ScopedArCoreObject<ArPose*> arcore_pose;
  ArPose_create(arcore_session_.get(), nullptr, arcore_pose.receive());
  if (!arcore_pose.is_valid()) {
    DLOG(ERROR) << "ArPose_create failed!";
    return nullptr;
  }

  ArCamera_getDisplayOrientedPose(arcore_session_.get(), arcore_camera.get(),
                                  arcore_pose.get());
  float pose_raw[7];  // 7 = orientation(4) + position(3).
  ArPose_getPoseRaw(arcore_session_.get(), arcore_pose.get(), pose_raw);

  mojom::VRPosePtr pose = mojom::VRPose::New();
  pose->orientation.emplace(pose_raw, pose_raw + 4);
  pose->position.emplace(pose_raw + 4, pose_raw + 7);

  return pose;
}

void ArCoreImpl::Pause() {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  ArStatus status = ArSession_pause(arcore_session_.get());
  DLOG_IF(ERROR, status != AR_SUCCESS)
      << "ArSession_pause failed: status = " << status;
}

void ArCoreImpl::Resume() {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  ArStatus status = ArSession_resume(arcore_session_.get());
  DLOG_IF(ERROR, status != AR_SUCCESS)
      << "ArSession_resume failed: status = " << status;
}

gfx::Transform ArCoreImpl::GetProjectionMatrix(float near, float far) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());

  internal::ScopedArCoreObject<ArCamera*> arcore_camera;
  ArFrame_acquireCamera(arcore_session_.get(), arcore_frame_.get(),
                        arcore_camera.receive());
  DCHECK(arcore_camera.is_valid())
      << "ArFrame_acquireCamera failed despite documentation saying it cannot";

  // ArCore's projection matrix is 16 floats in column-major order.
  float matrix_4x4[16];
  ArCamera_getProjectionMatrix(arcore_session_.get(), arcore_camera.get(), near,
                               far, matrix_4x4);
  gfx::Transform result;
  result.matrix().setColMajorf(matrix_4x4);
  return result;
}

// TODO(835948): remove image-size
bool ArCoreImpl::RequestHitTest(
    const mojom::XRRayPtr& ray,
    const gfx::Size& image_size,
    std::vector<mojom::XRHitResultPtr>* hit_results) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());

  internal::ScopedArCoreObject<ArHitResultList*> arcore_hit_result_list;
  ArHitResultList_create(arcore_session_.get(),
                         arcore_hit_result_list.receive());
  if (!arcore_hit_result_list.is_valid()) {
    DLOG(ERROR) << "ArHitResultList_create failed!";
    return false;
  }

  gfx::PointF screen_point;
  if (!TransformRayToScreenSpace(ray, image_size, &screen_point)) {
    return false;
  }

  // ArCore returns hit-results in sorted order, thus providing the guarantee
  // of sorted results promised by the WebXR spec for requestHitTest().
  ArFrame_hitTest(arcore_session_.get(), arcore_frame_.get(),
                  screen_point.x() * image_size.width(),
                  screen_point.y() * image_size.height(),
                  arcore_hit_result_list.get());

  int arcore_hit_result_list_size = 0;
  ArHitResultList_getSize(arcore_session_.get(), arcore_hit_result_list.get(),
                          &arcore_hit_result_list_size);

  // Go through the list in reverse order so the first hit we encounter is the
  // furthest.
  // We will accept the furthest hit and then for the rest require that the hit
  // be within the actual polygon detected by ArCore. This heuristic allows us
  // to get better results on floors w/o overestimating the size of tables etc.
  // See https://crbug.com/872855.

  for (int i = arcore_hit_result_list_size - 1; i >= 0; i--) {
    internal::ScopedArCoreObject<ArHitResult*> arcore_hit;

    ArHitResult_create(arcore_session_.get(), arcore_hit.receive());
    if (!arcore_hit.is_valid()) {
      DLOG(ERROR) << "ArHitResult_create failed!";
      return false;
    }

    ArHitResultList_getItem(arcore_session_.get(), arcore_hit_result_list.get(),
                            i, arcore_hit.get());

    internal::ScopedArCoreObject<ArTrackable*> ar_trackable;

    ArHitResult_acquireTrackable(arcore_session_.get(), arcore_hit.get(),
                                 ar_trackable.receive());
    ArTrackableType ar_trackable_type = AR_TRACKABLE_NOT_VALID;
    ArTrackable_getType(arcore_session_.get(), ar_trackable.get(),
                        &ar_trackable_type);

    // Only consider hits with plane trackables
    // TODO(874985): make this configurable or re-evaluate this decision
    if (AR_TRACKABLE_PLANE != ar_trackable_type) {
      continue;
    }

    internal::ScopedArCoreObject<ArPose*> arcore_pose;
    ArPose_create(arcore_session_.get(), nullptr, arcore_pose.receive());
    if (!arcore_pose.is_valid()) {
      DLOG(ERROR) << "ArPose_create failed!";
      return false;
    }

    ArHitResult_getHitPose(arcore_session_.get(), arcore_hit.get(),
                           arcore_pose.get());

    // After the first (furthest) hit, only return hits that are within the
    // actual detected polygon and not just within than the larger plane.
    if (!hit_results->empty()) {
      int32_t in_polygon = 0;
      ArPlane* ar_plane = ArAsPlane(ar_trackable.get());
      ArPlane_isPoseInPolygon(arcore_session_.get(), ar_plane,
                              arcore_pose.get(), &in_polygon);
      if (!in_polygon)
        continue;
    }

    mojom::XRHitResultPtr mojo_hit = mojom::XRHitResult::New();
    mojo_hit.get()->hit_matrix.resize(16);
    ArPose_getMatrix(arcore_session_.get(), arcore_pose.get(),
                     mojo_hit.get()->hit_matrix.data());

    // Insert new results at head to preserver order from ArCore
    hit_results->insert(hit_results->begin(), std::move(mojo_hit));
  }
  return true;
}

// TODO(835948): remove this method.
bool ArCoreImpl::TransformRayToScreenSpace(const mojom::XRRayPtr& ray,
                                           const gfx::Size& image_size,
                                           gfx::PointF* screen_point) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());

  internal::ScopedArCoreObject<ArCamera*> arcore_camera;
  ArFrame_acquireCamera(arcore_session_.get(), arcore_frame_.get(),
                        arcore_camera.receive());
  DCHECK(arcore_camera.is_valid())
      << "ArFrame_acquireCamera failed despite documentation saying it cannot";

  // Get the projection matrix.
  float projection_matrix[16];
  ArCamera_getProjectionMatrix(arcore_session_.get(), arcore_camera.get(), 0.1,
                               1000, projection_matrix);
  SkMatrix44 projection44;
  projection44.setColMajorf(projection_matrix);
  gfx::Transform projection_transform(projection44);

  // Get the view matrix.
  float view_matrix[16];
  ArCamera_getViewMatrix(arcore_session_.get(), arcore_camera.get(),
                         view_matrix);
  SkMatrix44 view44;
  view44.setColMajorf(view_matrix);
  gfx::Transform view_transform(view44);

  // Create the combined matrix.
  gfx::Transform proj_view_transform = projection_transform * view_transform;

  // Transform the ray into screen space.
  gfx::Point3F screen_point_3d = ray->origin + ray->direction;

  proj_view_transform.TransformPoint(&screen_point_3d);
  if (screen_point_3d.x() < -1 || screen_point_3d.x() > 1 ||
      screen_point_3d.y() < -1 || screen_point_3d.y() > 1) {
    // The point does not project back into screen space, so this won't
    // work with the screen-space-based hit-test API.
    DLOG(ERROR) << "Invalid ray - does not originate from device screen.";
    return false;
  }

  screen_point->set_x((screen_point_3d.x() + 1) / 2);
  // The calculated point in GL's normalized device coordinates (NDC) ranges
  // from -1..1, with -1, -1 at the bottom left of the screen, +1 at the top.
  // The output screen space coordinates range from 0..1, with 0, 0 at the
  // top left.
  screen_point->set_y((-screen_point_3d.y() + 1) / 2);
  return true;
}

bool ArCoreImpl::IsOnGlThread() {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

std::unique_ptr<ArCore> ArCoreImplFactory::Create() {
  return std::make_unique<ArCoreImpl>();
}

}  // namespace device
