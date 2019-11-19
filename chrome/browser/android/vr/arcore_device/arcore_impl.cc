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
#include "chrome/browser/android/vr/arcore_device/type_converters.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"

using base::android::JavaRef;

namespace {

std::pair<gfx::Quaternion, gfx::Point3F> GetPositionAndOrientationFromArPose(
    const ArSession* session,
    const device::internal::ScopedArCoreObject<ArPose*>& pose) {
  std::array<float, 7> pose_raw;  // 7 = orientation(4) + position(3).
  ArPose_getPoseRaw(session, pose.get(), pose_raw.data());

  return {gfx::Quaternion(pose_raw[0], pose_raw[1], pose_raw[2], pose_raw[3]),
          gfx::Point3F(pose_raw[4], pose_raw[5], pose_raw[6])};
}

// Helper, returns new VRPosePtr with position and orientation set to match the
// position and orientation of passed in |pose|.
device::mojom::VRPosePtr GetMojomVRPoseFromArPose(
    const ArSession* session,
    const device::internal::ScopedArCoreObject<ArPose*>& pose) {
  device::mojom::VRPosePtr result = device::mojom::VRPose::New();
  std::tie(result->orientation, result->position) =
      GetPositionAndOrientationFromArPose(session, pose);

  return result;
}

// Helper, returns new PosePtr with position and orientation set to match the
// position and orientation of passed in |pose|.
device::mojom::PosePtr GetMojomPoseFromArPose(
    const ArSession* session,
    const device::internal::ScopedArCoreObject<ArPose*>& pose) {
  device::mojom::PosePtr result = device::mojom::Pose::New();
  std::tie(result->orientation, result->position) =
      GetPositionAndOrientationFromArPose(session, pose);

  return result;
}

// Helper, creates new ArPose* with position and orientation set to match the
// position and orientation of passed in |pose|.
device::internal::ScopedArCoreObject<ArPose*> GetArPoseFromMojomPose(
    ArSession* session,
    const device::mojom::PosePtr& pose) {
  float pose_raw[7] = {};  // 7 = orientation(4) + position(3).

  pose_raw[0] = pose->orientation.x();
  pose_raw[1] = pose->orientation.y();
  pose_raw[2] = pose->orientation.z();
  pose_raw[3] = pose->orientation.w();

  pose_raw[4] = pose->position.x();
  pose_raw[5] = pose->position.y();
  pose_raw[6] = pose->position.z();

  device::internal::ScopedArCoreObject<ArPose*> result;

  ArPose_create(
      session, pose_raw,
      device::internal::ScopedArCoreObject<ArPose*>::Receiver(result).get());

  return result;
}

constexpr float kDefaultFloorHeightEstimation = 1.2;

}  // namespace

namespace device {

HitTestSubscriptionData::HitTestSubscriptionData(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    mojom::XRRayPtr ray)
    : native_origin_information(std::move(native_origin_information)),
      ray(std::move(ray)) {}

HitTestSubscriptionData::HitTestSubscriptionData(
    HitTestSubscriptionData&& other) = default;
HitTestSubscriptionData::~HitTestSubscriptionData() = default;

ArCoreImpl::ArCoreImpl()
    : gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

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

  ArStatus status = ArSession_create(
      env, context.obj(),
      internal::ScopedArCoreObject<ArSession*>::Receiver(session).get());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_create failed: " << status;
    return false;
  }

  // Set incognito mode for ARCore session - this is done unconditionally as we
  // always want to limit the amount of logging done by ARCore.
  ArSession_enableIncognitoMode_private(session.get());
  DVLOG(1) << __func__ << ": ARCore incognito mode enabled";

  internal::ScopedArCoreObject<ArConfig*> arcore_config;
  ArConfig_create(
      session.get(),
      internal::ScopedArCoreObject<ArConfig*>::Receiver(arcore_config).get());
  if (!arcore_config.is_valid()) {
    DLOG(ERROR) << "ArConfig_create failed";
    return false;
  }

  status = ArSession_configure(session.get(), arcore_config.get());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_configure failed: " << status;
    return false;
  }

  internal::ScopedArCoreObject<ArFrame*> frame;
  ArFrame_create(session.get(),
                 internal::ScopedArCoreObject<ArFrame*>::Receiver(frame).get());
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

  ArFrame_transformCoordinates2d(
      arcore_session_.get(), arcore_frame_.get(),
      AR_COORDINATES_2D_VIEW_NORMALIZED, num_elements / 2, &uvs[0],
      AR_COORDINATES_2D_TEXTURE_NORMALIZED, &uvs_out[0]);
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
  ArFrame_acquireCamera(
      arcore_session_.get(), arcore_frame_.get(),
      internal::ScopedArCoreObject<ArCamera*>::Receiver(arcore_camera).get());
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
  ArPose_create(
      arcore_session_.get(), nullptr,
      internal::ScopedArCoreObject<ArPose*>::Receiver(arcore_pose).get());
  if (!arcore_pose.is_valid()) {
    DLOG(ERROR) << "ArPose_create failed!";
    return nullptr;
  }

  ArCamera_getDisplayOrientedPose(arcore_session_.get(), arcore_camera.get(),
                                  arcore_pose.get());

  return GetMojomVRPoseFromArPose(arcore_session_.get(),
                                  std::move(arcore_pose));
}

void ArCoreImpl::EnsureArCorePlanesList() {
  if (!arcore_planes_.is_valid()) {
    ArTrackableList_create(
        arcore_session_.get(),
        internal::ScopedArCoreObject<ArTrackableList*>::Receiver(arcore_planes_)
            .get());
    DCHECK(arcore_planes_.is_valid());
  }
}

void ArCoreImpl::EnsureArCoreAnchorsList() {
  if (!arcore_anchors_.is_valid()) {
    ArAnchorList_create(
        arcore_session_.get(),
        internal::ScopedArCoreObject<ArAnchorList*>::Receiver(arcore_anchors_)
            .get());
    DCHECK(arcore_anchors_.is_valid());
  }
}

template <typename FunctionType>
void ArCoreImpl::ForEachArCorePlane(FunctionType fn) {
  DCHECK(arcore_planes_.is_valid());

  int32_t trackable_list_size;
  ArTrackableList_getSize(arcore_session_.get(), arcore_planes_.get(),
                          &trackable_list_size);

  for (int i = 0; i < trackable_list_size; i++) {
    internal::ScopedArCoreObject<ArTrackable*> trackable;
    ArTrackableList_acquireItem(
        arcore_session_.get(), arcore_planes_.get(), i,
        internal::ScopedArCoreObject<ArTrackable*>::Receiver(trackable).get());

    ArTrackingState tracking_state;
    ArTrackable_getTrackingState(arcore_session_.get(), trackable.get(),
                                 &tracking_state);

    if (tracking_state != ArTrackingState::AR_TRACKING_STATE_TRACKING) {
      // Skip all planes that are not currently tracked.
      continue;
    }

#if DCHECK_IS_ON()
    {
      ArTrackableType type;
      ArTrackable_getType(arcore_session_.get(), trackable.get(), &type);
      DCHECK(type == ArTrackableType::AR_TRACKABLE_PLANE)
          << "arcore_planes_ contains a trackable that is not an ArPlane!";
    }
#endif

    ArPlane* ar_plane =
        ArAsPlane(trackable.get());  // Naked pointer is fine here, ArAsPlane
                                     // does not increase ref count.

    internal::ScopedArCoreObject<ArPlane*> subsuming_plane;
    ArPlane_acquireSubsumedBy(
        arcore_session_.get(), ar_plane,
        internal::ScopedArCoreObject<ArPlane*>::Receiver(subsuming_plane)
            .get());

    if (subsuming_plane.is_valid()) {
      // Current plane was subsumed by other plane, skip this loop iteration.
      // Subsuming plane will be handled when its turn comes.
      continue;
    }

    fn(std::move(trackable), ar_plane);
  }
}  // namespace device

std::vector<mojom::XRAnchorDataPtr> ArCoreImpl::GetUpdatedAnchorsData() {
  EnsureArCoreAnchorsList();

  std::vector<mojom::XRAnchorDataPtr> result;

  ArFrame_getUpdatedAnchors(arcore_session_.get(), arcore_frame_.get(),
                            arcore_anchors_.get());

  ForEachArCoreAnchor([this, &result](ArAnchor* ar_anchor) {
    // pose
    internal::ScopedArCoreObject<ArPose*> anchor_pose;
    ArPose_create(
        arcore_session_.get(), nullptr,
        internal::ScopedArCoreObject<ArPose*>::Receiver(anchor_pose).get());
    ArAnchor_getPose(arcore_session_.get(), ar_anchor, anchor_pose.get());
    mojom::PosePtr pose =
        GetMojomPoseFromArPose(arcore_session_.get(), std::move(anchor_pose));

    // ID
    AnchorId anchor_id;
    bool created;
    std::tie(anchor_id, created) = CreateOrGetAnchorId(ar_anchor);

    DCHECK(!created)
        << "Anchor creation is app-initiated - we should never encounter an "
           "anchor that was created outside of `ArCoreImpl::CreateAnchor()`.";

    result.push_back(
        mojom::XRAnchorData::New(anchor_id.GetUnsafeValue(), std::move(pose)));
  });

  return result;
}

std::vector<uint64_t> ArCoreImpl::GetAllAnchorIds() {
  EnsureArCoreAnchorsList();

  std::vector<uint64_t> result;

  ArSession_getAllAnchors(arcore_session_.get(), arcore_anchors_.get());

  ForEachArCoreAnchor([this, &result](ArAnchor* ar_anchor) {
    // ID
    AnchorId anchor_id;
    bool created;
    std::tie(anchor_id, created) = CreateOrGetAnchorId(ar_anchor);

    DCHECK(!created)
        << "Anchor creation is app-initiated - we should never encounter an "
           "anchor that was created outside of `ArCoreImpl::CreateAnchor()`.";

    result.emplace_back(anchor_id);
  });

  return result;
}

template <typename FunctionType>
void ArCoreImpl::ForEachArCoreAnchor(FunctionType fn) {
  DCHECK(arcore_anchors_.is_valid());

  int32_t anchor_list_size;
  ArAnchorList_getSize(arcore_session_.get(), arcore_anchors_.get(),
                       &anchor_list_size);

  for (int i = 0; i < anchor_list_size; i++) {
    internal::ScopedArCoreObject<ArAnchor*> anchor;
    ArAnchorList_acquireItem(
        arcore_session_.get(), arcore_anchors_.get(), i,
        internal::ScopedArCoreObject<ArAnchor*>::Receiver(anchor).get());

    ArTrackingState tracking_state;
    ArAnchor_getTrackingState(arcore_session_.get(), anchor.get(),
                              &tracking_state);

    if (tracking_state != ArTrackingState::AR_TRACKING_STATE_TRACKING) {
      // Skip all anchors that are not currently tracked.
      continue;
    }

    fn(anchor.get());
  }
}

std::vector<mojom::XRPlaneDataPtr> ArCoreImpl::GetUpdatedPlanesData() {
  EnsureArCorePlanesList();

  std::vector<mojom::XRPlaneDataPtr> result;

  ArTrackableType plane_tracked_type = AR_TRACKABLE_PLANE;
  ArFrame_getUpdatedTrackables(arcore_session_.get(), arcore_frame_.get(),
                               plane_tracked_type, arcore_planes_.get());

  ForEachArCorePlane([this, &result](
                         internal::ScopedArCoreObject<ArTrackable*> trackable,
                         ArPlane* ar_plane) {
    // orientation
    ArPlaneType plane_type;
    ArPlane_getType(arcore_session_.get(), ar_plane, &plane_type);

    // pose
    internal::ScopedArCoreObject<ArPose*> plane_pose;
    ArPose_create(
        arcore_session_.get(), nullptr,
        internal::ScopedArCoreObject<ArPose*>::Receiver(plane_pose).get());
    ArPlane_getCenterPose(arcore_session_.get(), ar_plane, plane_pose.get());
    mojom::PosePtr pose =
        GetMojomPoseFromArPose(arcore_session_.get(), std::move(plane_pose));

    // polygon
    int32_t polygon_size;
    ArPlane_getPolygonSize(arcore_session_.get(), ar_plane, &polygon_size);
    // We are supposed to get 2*N floats describing (x, z) cooridinates of N
    // points.
    DCHECK(polygon_size % 2 == 0);

    std::unique_ptr<float[]> vertices_raw =
        std::make_unique<float[]>(polygon_size);
    ArPlane_getPolygon(arcore_session_.get(), ar_plane, vertices_raw.get());

    std::vector<mojom::XRPlanePointDataPtr> vertices;
    for (int i = 0; i < polygon_size; i += 2) {
      vertices.push_back(
          mojom::XRPlanePointData::New(vertices_raw[i], vertices_raw[i + 1]));
    }

    // ID
    PlaneId plane_id;
    bool created;
    std::tie(plane_id, created) = CreateOrGetPlaneId(ar_plane);

    result.push_back(mojom::XRPlaneData::New(
        plane_id.GetUnsafeValue(),
        mojo::ConvertTo<device::mojom::XRPlaneOrientation>(plane_type),
        std::move(pose), std::move(vertices)));
  });

  return result;
}

std::vector<uint64_t> ArCoreImpl::GetAllPlaneIds() {
  EnsureArCorePlanesList();

  std::vector<uint64_t> result;

  ArTrackableType plane_tracked_type = AR_TRACKABLE_PLANE;
  ArSession_getAllTrackables(arcore_session_.get(), plane_tracked_type,
                             arcore_planes_.get());

  std::map<PlaneId, device::internal::ScopedArCoreObject<ArTrackable*>>
      plane_id_to_plane_object;

  ForEachArCorePlane([this, &plane_id_to_plane_object, &result](
                         internal::ScopedArCoreObject<ArTrackable*> trackable,
                         ArPlane* ar_plane) {
    // ID
    PlaneId plane_id;
    bool created;
    std::tie(plane_id, created) = CreateOrGetPlaneId(ar_plane);

    DCHECK(!created)
        << "Newly detected planes should be handled by GetUpdatedPlanesData().";

    result.emplace_back(plane_id);
    plane_id_to_plane_object[plane_id] = std::move(trackable);
  });

  plane_id_to_plane_object_.swap(plane_id_to_plane_object);

  return result;
}

mojom::XRPlaneDetectionDataPtr ArCoreImpl::GetDetectedPlanesData() {
  TRACE_EVENT0("gpu", __FUNCTION__);

  auto updated_planes = GetUpdatedPlanesData();
  auto all_plane_ids = GetAllPlaneIds();

  return mojom::XRPlaneDetectionData::New(all_plane_ids,
                                          std::move(updated_planes));
}

mojom::XRAnchorsDataPtr ArCoreImpl::GetAnchorsData() {
  TRACE_EVENT0("gpu", __FUNCTION__);

  auto updated_anchors = GetUpdatedAnchorsData();
  auto all_anchor_ids = GetAllAnchorIds();

  return mojom::XRAnchorsData::New(all_anchor_ids, std::move(updated_anchors));
}

std::pair<PlaneId, bool> ArCoreImpl::CreateOrGetPlaneId(void* plane_address) {
  auto it = ar_plane_address_to_id_.find(plane_address);
  if (it != ar_plane_address_to_id_.end()) {
    return std::make_pair(it->second, false);
  }

  CHECK(next_id_ != std::numeric_limits<uint64_t>::max())
      << "preventing ID overflow";

  uint64_t current_id = next_id_++;
  ar_plane_address_to_id_.emplace(plane_address, current_id);

  return std::make_pair(PlaneId(current_id), true);
}

std::pair<AnchorId, bool> ArCoreImpl::CreateOrGetAnchorId(
    void* anchor_address) {
  auto it = ar_anchor_address_to_id_.find(anchor_address);
  if (it != ar_anchor_address_to_id_.end()) {
    return std::make_pair(it->second, false);
  }

  CHECK(next_id_ != std::numeric_limits<uint64_t>::max())
      << "preventing ID overflow";

  uint64_t current_id = next_id_++;
  ar_anchor_address_to_id_.emplace(anchor_address, current_id);

  return std::make_pair(AnchorId(current_id), true);
}

void ArCoreImpl::Pause() {
  DVLOG(2) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());

  ArStatus status = ArSession_pause(arcore_session_.get());
  DLOG_IF(ERROR, status != AR_SUCCESS)
      << "ArSession_pause failed: status = " << status;
}

void ArCoreImpl::Resume() {
  DVLOG(2) << __func__;

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
  ArFrame_acquireCamera(
      arcore_session_.get(), arcore_frame_.get(),
      internal::ScopedArCoreObject<ArCamera*>::Receiver(arcore_camera).get());
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

float ArCoreImpl::GetEstimatedFloorHeight() {
  return kDefaultFloorHeightEstimation;
}

base::Optional<uint64_t> ArCoreImpl::SubscribeToHitTest(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    mojom::XRRayPtr ray) {
  // First, check if we recognize the type of the native origin.

  if (native_origin_information->is_reference_space_category()) {
    // Reference spaces are implicitly recognized and don't carry an ID.
  } else if (native_origin_information->is_input_source_id()) {
    // Input source IDs are verified in the higher layer as ArCoreImpl does
    // not carry input source state.
  } else if (native_origin_information->is_plane_id()) {
    // Validate that we know which plane's space the hit test is interested in
    // tracking.
    if (plane_id_to_plane_object_.count(
            PlaneId(native_origin_information->get_plane_id())) == 0) {
      return base::nullopt;
    }
  } else if (native_origin_information->is_anchor_id()) {
    // Validate that we know which anchor's space the hit test is interested
    // in tracking.
    if (anchor_id_to_anchor_object_.count(
            AnchorId(native_origin_information->get_anchor_id())) == 0) {
      return base::nullopt;
    }
  } else {
    NOTREACHED();
    return base::nullopt;
  }

  auto subscription_id = CreateHitTestSubscriptionId();

  hit_test_subscription_id_to_data_.emplace(
      subscription_id,
      HitTestSubscriptionData{std::move(native_origin_information),
                              std::move(ray)});

  return subscription_id.GetUnsafeValue();
}

mojom::XRHitTestSubscriptionResultsDataPtr
ArCoreImpl::GetHitTestSubscriptionResults(
    const gfx::Transform& mojo_from_viewer,
    const base::Optional<std::vector<mojom::XRInputSourceStatePtr>>&
        maybe_input_state) {
  mojom::XRHitTestSubscriptionResultsDataPtr result =
      mojom::XRHitTestSubscriptionResultsData::New();

  for (auto& subscription_id_and_data : hit_test_subscription_id_to_data_) {
    // First, check if we can find the current transformation for a ray. If not,
    // skip processing this subscription.
    auto maybe_mojo_from_native_origin = GetMojoFromNativeOrigin(
        subscription_id_and_data.second.native_origin_information,
        mojo_from_viewer, maybe_input_state);

    if (!maybe_mojo_from_native_origin) {
      continue;
    }

    // Since we have a transform, let's use it to obtain hit test results.
    result->results.push_back(GetHitTestSubscriptionResult(
        HitTestSubscriptionId(subscription_id_and_data.first),
        *subscription_id_and_data.second.ray, *maybe_mojo_from_native_origin));
  }

  return result;
}

device::mojom::XRHitTestSubscriptionResultDataPtr
ArCoreImpl::GetHitTestSubscriptionResult(
    HitTestSubscriptionId id,
    const mojom::XRRay& native_origin_ray,
    const gfx::Transform& mojo_from_native_origin) {
  // Transform the ray according to the latest transform based on the XRSpace
  // used in hit test subscription.

  gfx::Point3F origin = native_origin_ray.origin;
  mojo_from_native_origin.TransformPoint(&origin);

  gfx::Vector3dF direction = native_origin_ray.direction;
  mojo_from_native_origin.TransformVector(&direction);

  std::vector<mojom::XRHitResultPtr> hit_results;
  RequestHitTest(origin, direction, &hit_results);

  return mojom::XRHitTestSubscriptionResultData::New(id.GetUnsafeValue(),
                                                     std::move(hit_results));
}

base::Optional<gfx::Transform> ArCoreImpl::GetMojoFromReferenceSpace(
    device::mojom::XRReferenceSpaceCategory category,
    const gfx::Transform& mojo_from_viewer) {
  switch (category) {
    case device::mojom::XRReferenceSpaceCategory::LOCAL:
      return gfx::Transform{};
    case device::mojom::XRReferenceSpaceCategory::LOCAL_FLOOR: {
      auto result = gfx::Transform{};
      result.Translate3d(0, -GetEstimatedFloorHeight(), 0);
      return result;
    }
    case device::mojom::XRReferenceSpaceCategory::VIEWER:
      return mojo_from_viewer;
    case device::mojom::XRReferenceSpaceCategory::BOUNDED_FLOOR:
      return base::nullopt;
    case device::mojom::XRReferenceSpaceCategory::UNBOUNDED:
      return base::nullopt;
  }
}

base::Optional<gfx::Transform> ArCoreImpl::GetMojoFromNativeOrigin(
    const mojom::XRNativeOriginInformationPtr& native_origin_information,
    const gfx::Transform& mojo_from_viewer,
    const base::Optional<std::vector<mojom::XRInputSourceStatePtr>>&
        maybe_input_state) {
  if (native_origin_information->is_input_source_id()) {
    if (!maybe_input_state) {
      return base::nullopt;
    }

    // Linear search should be fine for ARCore device as it only has one input
    // source (for now).
    for (auto& input_source_state : *maybe_input_state) {
      if (input_source_state->source_id ==
          native_origin_information->get_input_source_id()) {
        if (!input_source_state->description->input_from_pointer) {
          return base::nullopt;
        }

        auto viewer_from_pointer =
            *input_source_state->description->input_from_pointer;

        return mojo_from_viewer * viewer_from_pointer;
      }
    }

    return base::nullopt;
  } else if (native_origin_information->is_reference_space_category()) {
    return GetMojoFromReferenceSpace(
        native_origin_information->get_reference_space_category(),
        mojo_from_viewer);
  } else if (native_origin_information->is_plane_id()) {
    auto plane_it = plane_id_to_plane_object_.find(
        PlaneId(native_origin_information->get_plane_id()));
    if (plane_it == plane_id_to_plane_object_.end()) {
      return base::nullopt;
    }

    // Naked pointer is fine as we don't own the object and ArAsPlane does not
    // increase the refcount.
    ArPlane* plane = ArAsPlane(plane_it->second.get());

    internal::ScopedArCoreObject<ArPose*> ar_pose;
    ArPose_create(
        arcore_session_.get(), nullptr,
        internal::ScopedArCoreObject<ArPose*>::Receiver(ar_pose).get());
    ArPlane_getCenterPose(arcore_session_.get(), plane, ar_pose.get());
    mojom::PosePtr mojo_pose =
        GetMojomPoseFromArPose(arcore_session_.get(), std::move(ar_pose));

    return mojo::ConvertTo<gfx::Transform>(mojo_pose);
  } else if (native_origin_information->is_anchor_id()) {
    auto anchor_it = anchor_id_to_anchor_object_.find(
        AnchorId(native_origin_information->get_anchor_id()));
    if (anchor_it == anchor_id_to_anchor_object_.end()) {
      return base::nullopt;
    }

    internal::ScopedArCoreObject<ArPose*> ar_pose;
    ArPose_create(
        arcore_session_.get(), nullptr,
        internal::ScopedArCoreObject<ArPose*>::Receiver(ar_pose).get());

    ArAnchor_getPose(arcore_session_.get(), anchor_it->second.get(),
                     ar_pose.get());
    mojom::PosePtr mojo_pose =
        GetMojomPoseFromArPose(arcore_session_.get(), std::move(ar_pose));

    return mojo::ConvertTo<gfx::Transform>(mojo_pose);
  } else {
    NOTREACHED();
    return base::nullopt;
  }
}  // namespace device

void ArCoreImpl::UnsubscribeFromHitTest(uint64_t subscription_id) {
  auto it = hit_test_subscription_id_to_data_.find(
      HitTestSubscriptionId(subscription_id));
  if (it == hit_test_subscription_id_to_data_.end()) {
    return;
  }

  hit_test_subscription_id_to_data_.erase(it);
}

HitTestSubscriptionId ArCoreImpl::CreateHitTestSubscriptionId() {
  CHECK(next_id_ != std::numeric_limits<uint64_t>::max())
      << "preventing ID overflow";

  uint64_t current_id = next_id_++;

  return HitTestSubscriptionId(current_id);
}

bool ArCoreImpl::RequestHitTest(
    const mojom::XRRayPtr& ray,
    std::vector<mojom::XRHitResultPtr>* hit_results) {
  DCHECK(ray);
  return RequestHitTest(ray->origin, ray->direction, hit_results);
}

bool ArCoreImpl::RequestHitTest(
    const gfx::Point3F& origin,
    const gfx::Vector3dF& direction,
    std::vector<mojom::XRHitResultPtr>* hit_results) {
  DVLOG(2) << __func__ << ": origin=" << origin.ToString()
           << ", direction=" << direction.ToString();

  DCHECK(hit_results);
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());

  // ArCore returns hit-results in sorted order, thus providing the guarantee
  // of sorted results promised by the WebXR spec for requestHitTest().
  std::array<float, 3> origin_array = {origin.x(), origin.y(), origin.z()};
  std::array<float, 3> direction_array = {direction.x(), direction.y(),
                                          direction.z()};

  internal::ScopedArCoreObject<ArHitResultList*> arcore_hit_result_list;
  ArHitResultList_create(
      arcore_session_.get(),
      internal::ScopedArCoreObject<ArHitResultList*>::Receiver(
          arcore_hit_result_list)
          .get());
  if (!arcore_hit_result_list.is_valid()) {
    DLOG(ERROR) << "ArHitResultList_create failed!";
    return false;
  }

  ArFrame_hitTestRay(arcore_session_.get(), arcore_frame_.get(),
                     origin_array.data(), direction_array.data(),
                     arcore_hit_result_list.get());

  int arcore_hit_result_list_size = 0;
  ArHitResultList_getSize(arcore_session_.get(), arcore_hit_result_list.get(),
                          &arcore_hit_result_list_size);
  DVLOG(2) << __func__
           << ": arcore_hit_result_list_size=" << arcore_hit_result_list_size;

  // Go through the list in reverse order so the first hit we encounter is the
  // furthest.
  // We will accept the furthest hit and then for the rest require that the hit
  // be within the actual polygon detected by ArCore. This heuristic allows us
  // to get better results on floors w/o overestimating the size of tables etc.
  // See https://crbug.com/872855.

  for (int i = arcore_hit_result_list_size - 1; i >= 0; i--) {
    internal::ScopedArCoreObject<ArHitResult*> arcore_hit;

    ArHitResult_create(
        arcore_session_.get(),
        internal::ScopedArCoreObject<ArHitResult*>::Receiver(arcore_hit).get());

    if (!arcore_hit.is_valid()) {
      DLOG(ERROR) << "ArHitResult_create failed!";
      return false;
    }

    ArHitResultList_getItem(arcore_session_.get(), arcore_hit_result_list.get(),
                            i, arcore_hit.get());

    internal::ScopedArCoreObject<ArTrackable*> ar_trackable;

    ArHitResult_acquireTrackable(
        arcore_session_.get(), arcore_hit.get(),
        internal::ScopedArCoreObject<ArTrackable*>::Receiver(ar_trackable)
            .get());
    ArTrackableType ar_trackable_type = AR_TRACKABLE_NOT_VALID;
    ArTrackable_getType(arcore_session_.get(), ar_trackable.get(),
                        &ar_trackable_type);

    // Only consider hits with plane trackables
    // TODO(874985): make this configurable or re-evaluate this decision
    if (AR_TRACKABLE_PLANE != ar_trackable_type) {
      DVLOG(2) << __func__
               << ": hit a trackable that is not a plane, ignoring it";
      continue;
    }

    internal::ScopedArCoreObject<ArPose*> arcore_pose;
    ArPose_create(
        arcore_session_.get(), nullptr,
        internal::ScopedArCoreObject<ArPose*>::Receiver(arcore_pose).get());
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
      if (!in_polygon) {
        DVLOG(2) << __func__
                 << ": hit a trackable that is not within detected polygon, "
                    "ignoring it";
        continue;
      }
    }

    std::array<float, 16> matrix;
    ArPose_getMatrix(arcore_session_.get(), arcore_pose.get(), matrix.data());

    mojom::XRHitResultPtr mojo_hit = mojom::XRHitResult::New();

    // ArPose_getMatrix returns the matrix in WebGL style column-major order
    // and gfx::Transform expects row major order.
    // clang-format off
    mojo_hit->hit_matrix = gfx::Transform(
      matrix[0], matrix[4], matrix[8],  matrix[12],
      matrix[1], matrix[5], matrix[9],  matrix[13],
      matrix[2], matrix[6], matrix[10], matrix[14],
      matrix[3], matrix[7], matrix[11], matrix[15]
    );
    // clang-format on

    // Insert new results at head to preserver order from ArCore
    hit_results->insert(hit_results->begin(), std::move(mojo_hit));
  }

  DVLOG(2) << __func__ << ": hit_results->size()=" << hit_results->size();
  return true;
}

base::Optional<uint64_t> ArCoreImpl::CreateAnchor(
    const device::mojom::PosePtr& pose) {
  DCHECK(pose);

  auto ar_pose = GetArPoseFromMojomPose(arcore_session_.get(), pose);

  device::internal::ScopedArCoreObject<ArAnchor*> ar_anchor;
  ArSession_acquireNewAnchor(
      arcore_session_.get(), ar_pose.get(),
      device::internal::ScopedArCoreObject<ArAnchor*>::Receiver(ar_anchor)
          .get());

  AnchorId anchor_id;
  bool created;
  std::tie(anchor_id, created) = CreateOrGetAnchorId(ar_anchor.get());

  DCHECK(created) << "This should always be a new anchor, not something we've "
                     "seen previously.";

  anchor_id_to_anchor_object_[anchor_id] = std::move(ar_anchor);

  return anchor_id.GetUnsafeValue();
}

base::Optional<uint64_t> ArCoreImpl::CreateAnchor(
    const device::mojom::PosePtr& pose,
    uint64_t plane_id) {
  DCHECK(pose);

  auto ar_pose = GetArPoseFromMojomPose(arcore_session_.get(), pose);

  auto it = plane_id_to_plane_object_.find(PlaneId(plane_id));
  if (it == plane_id_to_plane_object_.end()) {
    return base::nullopt;
  }

  device::internal::ScopedArCoreObject<ArAnchor*> ar_anchor;
  ArTrackable_acquireNewAnchor(
      arcore_session_.get(), it->second.get(), ar_pose.get(),
      device::internal::ScopedArCoreObject<ArAnchor*>::Receiver(ar_anchor)
          .get());

  AnchorId anchor_id;
  bool created;
  std::tie(anchor_id, created) = CreateOrGetAnchorId(ar_anchor.get());

  DCHECK(created) << "This should always be a new anchor, not something we've "
                     "seen previously.";

  anchor_id_to_anchor_object_[anchor_id] = std::move(ar_anchor);

  return anchor_id.GetUnsafeValue();
}

void ArCoreImpl::DetachAnchor(uint64_t anchor_id) {
  auto it = anchor_id_to_anchor_object_.find(AnchorId(anchor_id));
  if (it == anchor_id_to_anchor_object_.end()) {
    return;
  }

  ArAnchor_detach(arcore_session_.get(), it->second.get());

  anchor_id_to_anchor_object_.erase(it);
}

bool ArCoreImpl::IsOnGlThread() {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

std::unique_ptr<ArCore> ArCoreImplFactory::Create() {
  return std::make_unique<ArCoreImpl>();
}

}  // namespace device
