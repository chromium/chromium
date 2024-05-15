// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/fake_arcore.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/containers/contains.h"
#include "base/numerics/angle_conversions.h"
#include "base/task/single_thread_task_runner.h"

namespace {}

namespace device {

FakeArCore::FakeArCore()
    : gl_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()) {}

FakeArCore::~FakeArCore() = default;

ArCore::MinMaxRange FakeArCore::GetTargetFramerateRange() {
  return {30.f, 30.f};
}

std::optional<ArCore::InitializeResult> FakeArCore::Initialize(
    base::android::ScopedJavaLocalRef<jobject> application_context,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        required_features,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        optional_features,
    const std::vector<device::mojom::XRTrackedImagePtr>& tracked_images,
    std::optional<ArCore::DepthSensingConfiguration> depth_sensing_config) {
  DCHECK(IsOnGlThread());

  std::unordered_set<device::mojom::XRSessionFeature> enabled_features;
  enabled_features.insert(required_features.begin(), required_features.end());
  enabled_features.insert(optional_features.begin(), optional_features.end());

  // Fake device does not support depth for now:
  if (base::Contains(required_features,
                     device::mojom::XRSessionFeature::DEPTH)) {
    return std::nullopt;
  }

  if (base::Contains(optional_features,
                     device::mojom::XRSessionFeature::DEPTH)) {
    enabled_features.erase(device::mojom::XRSessionFeature::DEPTH);
  }

  return ArCore::InitializeResult(enabled_features, std::nullopt);
}

void FakeArCore::SetDisplayGeometry(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation) {
  DCHECK(IsOnGlThread());

  display_rotation_ = display_rotation;
  frame_size_ = frame_size;
}

gfx::Size FakeArCore::GetUncroppedCameraImageSize() const {
  return {1920, 960};
}

void FakeArCore::SetCameraTexture(uint32_t texture) {
  DCHECK(IsOnGlThread());
  // This is a no-op for the FakeArCore implementation. We might want to
  // store the textureid for use in unit tests, but currently ArCoreDeviceTest
  // is using mocked image transport so the actual texture doesn't have to
  // be set. Current approach won't work for tests that rely on the texture.

  DVLOG(2) << __FUNCTION__ << ": camera texture=" << texture;
}

std::vector<float> FakeArCore::TransformDisplayUvCoords(
    const base::span<const float> uvs) const {
  // Try to match ArCore's transform values.
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
  // TODO(crbug.com/40877372): This logic is quite complicated,
  // and the current arcore_device_unittest doesn't really care about
  // the details.

  // SetDisplayGeometry should have been called first.
  DCHECK(frame_size_.width());
  DCHECK(frame_size_.height());

  DCHECK_GE(uvs.size(), 6u);

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
  float base_tan = tanf(base::DegToRad(fov_half_angle_degrees));
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
  result.set_rc(0, 0, 1.f / right_tan);
  result.set_rc(1, 1, 1.f / up_tan);
  result.set_rc(2, 2, (near + far) / (near - far));
  result.set_rc(3, 2, -1.0f);
  result.set_rc(2, 3, (2.0f * far * near) / (near - far));
  result.set_rc(3, 3, 0.0f);
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

base::TimeDelta FakeArCore::GetFrameTimestamp() {
  return base::TimeTicks::Now() - base::TimeTicks();
}

float FakeArCore::GetEstimatedFloorHeight() {
  return 2.0;
}

bool FakeArCore::RequestHitTest(
    const mojom::XRRayPtr& ray,
    std::vector<mojom::XRHitResultPtr>* hit_results) {
  mojom::XRHitResultPtr hit = mojom::XRHitResult::New();
  // Default-constructed `hit->mojo_from_result` is fine, no need to set
  // anything.
  hit_results->push_back(std::move(hit));

  return true;
}

std::optional<uint64_t> FakeArCore::SubscribeToHitTest(
    mojom::XRNativeOriginInformationPtr nativeOriginInformation,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray) {
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

std::optional<uint64_t> FakeArCore::SubscribeToHitTestForTransientInput(
    const std::string& profile_name,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray) {
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

mojom::XRHitTestSubscriptionResultsDataPtr
FakeArCore::GetHitTestSubscriptionResults(
    const gfx::Transform& mojo_from_viewer,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  return nullptr;
}

void FakeArCore::UnsubscribeFromHitTest(uint64_t subscription_id) {
  NOTREACHED_IN_MIGRATION();
}

mojom::XRPlaneDetectionDataPtr FakeArCore::GetDetectedPlanesData() {
  std::vector<mojom::XRPlaneDataPtr> result;

  // 1m ahead of the origin, neutral orientation facing forward.
  device::Pose pose(gfx::Point3F(0.0, 0.0, -1.0), gfx::Quaternion());

  // some random triangle
  std::vector<mojom::XRPlanePointDataPtr> vertices;
  vertices.push_back(mojom::XRPlanePointData::New(-0.3, -0.3));
  vertices.push_back(mojom::XRPlanePointData::New(0, 0.3));
  vertices.push_back(mojom::XRPlanePointData::New(0.3, -0.3));

  result.push_back(
      mojom::XRPlaneData::New(1, device::mojom::XRPlaneOrientation::HORIZONTAL,
                              pose, std::move(vertices)));

  return mojom::XRPlaneDetectionData::New(std::vector<uint64_t>{1},
                                          std::move(result));
}

mojom::XRAnchorsDataPtr FakeArCore::GetAnchorsData() {
  std::vector<mojom::XRAnchorDataPtr> result;
  std::vector<uint64_t> result_ids;

  for (auto& anchor_id_and_data : anchors_) {
    device::Pose pose(anchor_id_and_data.second.position,
                      anchor_id_and_data.second.orientation);

    result.push_back(mojom::XRAnchorData::New(anchor_id_and_data.first, pose));
    result_ids.push_back(anchor_id_and_data.first);
  }

  return mojom::XRAnchorsData::New(std::move(result_ids), std::move(result));
}

mojom::XRLightEstimationDataPtr FakeArCore::GetLightEstimationData() {
  auto result = mojom::XRLightEstimationData::New();

  // Initialize light probe with a top-down white light
  result->light_probe = mojom::XRLightProbe::New();
  result->light_probe->main_light_direction = gfx::Vector3dF(0, -1, 0);
  result->light_probe->main_light_intensity = device::RgbTupleF32(1, 1, 1);

  // Initialize spherical harmonics to zero-filled array
  result->light_probe->spherical_harmonics = mojom::XRSphericalHarmonics::New();
  result->light_probe->spherical_harmonics->coefficients.resize(9);

  // Initialize reflection_probe to black
  result->reflection_probe = mojom::XRReflectionProbe::New();
  result->reflection_probe->cube_map = mojom::XRCubeMap::New();
  result->reflection_probe->cube_map->width_and_height = 16;
  result->reflection_probe->cube_map->positive_x.resize(16 * 16);
  result->reflection_probe->cube_map->negative_x.resize(16 * 16);
  result->reflection_probe->cube_map->positive_y.resize(16 * 16);
  result->reflection_probe->cube_map->negative_y.resize(16 * 16);
  result->reflection_probe->cube_map->positive_z.resize(16 * 16);
  result->reflection_probe->cube_map->negative_z.resize(16 * 16);

  return result;
}

mojom::XRDepthDataPtr FakeArCore::GetDepthData() {
  return nullptr;
}

void FakeArCore::CreatePlaneAttachedAnchor(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const device::Pose& native_origin_from_anchor,
    uint64_t plane_id,
    CreateAnchorCallback callback) {
  // TODO(crbug.com/41475117): Fix this when implementing tests.
  std::move(callback).Run(mojom::CreateAnchorResult::FAILURE, 0);
}

void FakeArCore::CreateAnchor(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const device::Pose& native_origin_from_anchor,
    CreateAnchorCallback callback) {
  std::move(callback).Run(mojom::CreateAnchorResult::FAILURE, 0);
}

void FakeArCore::ProcessAnchorCreationRequests(
    const gfx::Transform& mojo_from_viewer,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state,
    const base::TimeTicks& frame_time) {
  // No-op - nothing gets deferred so far.
}

void FakeArCore::DetachAnchor(uint64_t anchor_id) {
  auto count = anchors_.erase(anchor_id);
  DCHECK_EQ(1u, count);
}

mojom::XRTrackedImagesDataPtr FakeArCore::GetTrackedImages() {
  std::vector<mojom::XRTrackedImageDataPtr> images_data;
  return mojom::XRTrackedImagesData::New(std::move(images_data), std::nullopt);
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
