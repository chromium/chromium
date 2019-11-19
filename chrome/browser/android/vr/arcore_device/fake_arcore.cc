// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/fake_arcore.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/numerics/math_constants.h"
#include "base/single_thread_task_runner.h"
#include "ui/display/display.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

namespace {}

namespace device {

FakeArCore::FakeArCore()
    : gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

FakeArCore::~FakeArCore() = default;

bool FakeArCore::Initialize(
    base::android::ScopedJavaLocalRef<jobject> application_context) {
  DCHECK(IsOnGlThread());
  return true;
}

void FakeArCore::SetDisplayGeometry(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation) {
  DCHECK(IsOnGlThread());

  display_rotation_ = display_rotation;
  frame_size_ = frame_size;
}

void FakeArCore::SetCameraTexture(GLuint texture) {
  DCHECK(IsOnGlThread());
  // This is a no-op for the FakeArCore implementation. We might want to
  // store the textureid for use in unit tests, but currently ArCoreDeviceTest
  // is using mocked image transport so the actual texture doesn't have to
  // be set. Current approach won't work for tests that rely on the texture.

  DVLOG(2) << __FUNCTION__ << ": camera texture=" << texture;
}

std::vector<float> FakeArCore::TransformDisplayUvCoords(
    const base::span<const float> uvs) {
  // Try to match ArCore's transfore values.
  //
  // Sample ArCore input: width=1080, height=1795, rotation=0,
  // vecs = (0, 0), (0, 1), (1, 0), (1, 1)
  // Sample ArCore output:
  //   (0.0325544, 1,
  //    0.967446, 1,
  //    0.0325543, 0,
  //    0.967446, 1.19209e-07)
  //
  // FakeArCoreDriver test_arcore;
  // test_arcore.SetCameraAspect(16.f / 9.f);
  // test_arcore.SetDisplayGeometry(0, 1080, 1795);
  // float in[8] = {0, 0, 0, 1, 1, 0, 1, 1};
  // float out[8];
  // test_arcore.TransformDisplayUvCoords(8, &in[0], &out[0]);
  //
  // Fake driver result:
  // TransformDisplayUvCoords: too wide. v0=0.0325521 vrange=0.934896
  //    uv[0]=(0.0325521, 1)
  //    uv[2]=(0.967448, 1)
  //    uv[4]=(0.0325521, 0)
  //    uv[6]=(0.967448, 0)
  //
  // TODO(klausw): move this to a unittest.

  // SetDisplayGeometry should have been called first.
  DCHECK(frame_size_.width());
  DCHECK(frame_size_.height());

  // Do clipping calculations in orientation ROTATE_0. screen U is left=0,
  // right=1. Screen V is bottom=0, top=1. We'll apply screen rotation later.

  float display_aspect =
      static_cast<float>(frame_size_.width()) / frame_size_.height();
  float target_aspect = camera_aspect_;

  // Simulate that the fake camera is rotated by 90 degrees from the usual
  // convention to match the Pixel camera. If the screen is in portrait mode
  // (ROTATION_0), the camera's UV origin is around the bottom right of the
  // screen, with camera +u going left and camera +v going up on the screen. So
  // use the original camera aspect in landscape/seascape, and the inverse
  // for portrait/upside-down orientation.
  if (display_rotation_ == display::Display::Rotation::ROTATE_0 ||
      display_rotation_ == display::Display::Rotation::ROTATE_180) {
    target_aspect = 1.0f / target_aspect;
  }
  DVLOG(3) << __FUNCTION__ << ": rotation=" << display_rotation_
           << " frame_size_.width=" << frame_size_.width()
           << " frame_size_.height=" << frame_size_.height()
           << " display_aspect=" << display_aspect
           << " target_aspect=" << target_aspect;

  float u0 = 0;
  float v0 = 0;
  float urange = 1;
  float vrange = 1;

  if (display_aspect > target_aspect) {
    // Display too wide. Fill width, crop V evenly at top/bottom.
    vrange = target_aspect / display_aspect;
    v0 = (1.f - vrange) / 2;
    DVLOG(3) << ": too wide. v0=" << v0 << " vrange=" << vrange;
  } else {
    // Display too narrow. Fill height, crop U evenly at sides.
    urange = display_aspect / target_aspect;
    u0 = (1.f - urange) / 2;
    DVLOG(3) << ": too narrow. u0=" << u0 << " urange=" << urange;
  }

  size_t num_elements = uvs.size();
  DCHECK(num_elements % 2 == 0);
  std::vector<float> uvs_out;
  uvs_out.reserve(num_elements);
  for (size_t i = 0; i < num_elements; i += 2) {
    float x = uvs[i];
    float y = uvs[i + 1];
    float u, v;
    switch (display_rotation_) {
      case display::Display::Rotation::ROTATE_90:
        u = u0 + x * urange;
        v = v0 + y * vrange;
        break;
      case display::Display::Rotation::ROTATE_180:
        u = 1.f - (v0 + y * vrange);
        v = u0 + x * urange;
        break;
      case display::Display::Rotation::ROTATE_270:
        u = 1.f - (u0 + x * urange);
        v = 1.f - (v0 + y * vrange);
        break;
      case display::Display::Rotation::ROTATE_0:
        u = v0 + y * vrange;
        v = 1.f - (u0 + x * urange);
        break;
    }
    uvs_out.push_back(u);
    uvs_out.push_back(v);
    DVLOG(2) << __FUNCTION__ << ": uv[" << i << "]=(" << u << ", " << v << ")";
  }
  return uvs_out;
}

gfx::Transform FakeArCore::GetProjectionMatrix(float near, float far) {
  DCHECK(IsOnGlThread());
  // Get a projection matrix matching the current screen orientation and
  // aspect. Currently, this uses a hardcoded FOV angle for the smaller screen
  // dimension, and adjusts the other angle to preserve the aspect. A better
  // simulation of ArCore should apply cropping to the underlying fixed-aspect
  // simulated camera image.
  constexpr float fov_half_angle_degrees = 30.f;
  float base_tan = tanf(fov_half_angle_degrees * base::kPiFloat / 180.f);
  float right_tan;
  float up_tan;
  if (display_rotation_ == display::Display::Rotation::ROTATE_0 ||
      display_rotation_ == display::Display::Rotation::ROTATE_180) {
    // portrait aspect
    right_tan = base_tan;
    up_tan = base_tan * frame_size_.height() / frame_size_.width();
  } else {
    // landscape aspect
    up_tan = base_tan;
    right_tan = base_tan * frame_size_.width() / frame_size_.height();
  }
  // Calculate a perspective matrix based on the FOV values.
  gfx::Transform result;
  result.matrix().set(0, 0, 1.f / right_tan);
  result.matrix().set(1, 1, 1.f / up_tan);
  result.matrix().set(2, 2, (near + far) / (near - far));
  result.matrix().set(3, 2, -1.0f);
  result.matrix().set(2, 3, (2.0f * far * near) / (near - far));
  result.matrix().set(3, 3, 0.0f);
  return result;
}

mojom::VRPosePtr FakeArCore::Update(bool* camera_updated) {
  DCHECK(IsOnGlThread());
  DCHECK(camera_updated);

  *camera_updated = true;

  // 1m up from the origin, neutral orientation facing forward.
  mojom::VRPosePtr pose = mojom::VRPose::New();
  pose->position = gfx::Point3F(0.0, 1.0, 0.0);
  pose->orientation = gfx::Quaternion();

  return pose;
}

float FakeArCore::GetEstimatedFloorHeight() {
  return 2.0;
}

bool FakeArCore::RequestHitTest(
    const mojom::XRRayPtr& ray,
    std::vector<mojom::XRHitResultPtr>* hit_results) {
  mojom::XRHitResultPtr hit = mojom::XRHitResult::New();
  // Identity matrix - no translation and default orientation.
  hit->hit_matrix = gfx::Transform();
  hit_results->push_back(std::move(hit));

  return true;
}

base::Optional<uint64_t> FakeArCore::SubscribeToHitTest(
    mojom::XRNativeOriginInformationPtr nativeOriginInformation,
    mojom::XRRayPtr ray) {
  NOTREACHED();
  return base::nullopt;
}

mojom::XRHitTestSubscriptionResultsDataPtr
FakeArCore::GetHitTestSubscriptionResults(
    const gfx::Transform& mojo_from_viewer,
    const base::Optional<std::vector<mojom::XRInputSourceStatePtr>>&
        maybe_input_state) {
  return nullptr;
}

void FakeArCore::UnsubscribeFromHitTest(uint64_t subscription_id) {
  NOTREACHED();
}

mojom::XRPlaneDetectionDataPtr FakeArCore::GetDetectedPlanesData() {
  std::vector<mojom::XRPlaneDataPtr> result;

  // 1m ahead of the origin, neutral orientation facing forward.
  mojom::PosePtr pose = mojom::Pose::New();
  pose->position = gfx::Point3F(0.0, 0.0, -1.0);
  pose->orientation = gfx::Quaternion();

  // some random triangle
  std::vector<mojom::XRPlanePointDataPtr> vertices;
  vertices.push_back(mojom::XRPlanePointData::New(-0.3, -0.3));
  vertices.push_back(mojom::XRPlanePointData::New(0, 0.3));
  vertices.push_back(mojom::XRPlanePointData::New(0.3, -0.3));

  result.push_back(
      mojom::XRPlaneData::New(1, device::mojom::XRPlaneOrientation::HORIZONTAL,
                              std::move(pose), std::move(vertices)));

  return mojom::XRPlaneDetectionData::New(std::vector<uint64_t>{1},
                                          std::move(result));
}

mojom::XRAnchorsDataPtr FakeArCore::GetAnchorsData() {
  std::vector<mojom::XRAnchorDataPtr> result;
  std::vector<uint64_t> result_ids;

  for (auto& anchor_id_and_data : anchors_) {
    mojom::PosePtr pose = mojom::Pose::New();
    pose->position = anchor_id_and_data.second.position;
    pose->orientation = anchor_id_and_data.second.orientation;

    result.push_back(
        mojom::XRAnchorData::New(anchor_id_and_data.first, std::move(pose)));
    result_ids.push_back(anchor_id_and_data.first);
  }

  return mojom::XRAnchorsData::New(std::move(result_ids), std::move(result));
}

base::Optional<uint64_t> FakeArCore::CreateAnchor(const mojom::PosePtr& pose,
                                                  uint64_t plane_id) {
  // TODO(992035): Fix this when implementing tests.
  return CreateAnchor(pose);
}

base::Optional<uint64_t> FakeArCore::CreateAnchor(const mojom::PosePtr& pose) {
  DCHECK(pose);

  anchors_[next_id_] = {pose->position, pose->orientation};
  int32_t anchor_id = next_id_;

  next_id_++;

  return anchor_id;
}

void FakeArCore::DetachAnchor(uint64_t anchor_id) {
  auto count = anchors_.erase(anchor_id);
  DCHECK_EQ(1u, count);
}

void FakeArCore::Pause() {
  DCHECK(IsOnGlThread());
}

void FakeArCore::Resume() {
  DCHECK(IsOnGlThread());
}

bool FakeArCore::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

std::unique_ptr<ArCore> FakeArCoreFactory::Create() {
  return std::make_unique<FakeArCore>();
}

}  // namespace device
