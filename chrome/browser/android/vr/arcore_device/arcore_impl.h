// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_generic.h"
#include "base/util/type_safety/id_type.h"
#include "chrome/browser/android/vr/arcore_device/arcore.h"
#include "chrome/browser/android/vr/arcore_device/arcore_sdk.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace device {

namespace internal {

template <class T>
struct ScopedGenericArObject {
  static T InvalidValue() { return nullptr; }
  static void Free(T object) {}
};

template <>
void inline ScopedGenericArObject<ArSession*>::Free(ArSession* ar_session) {
  ArSession_destroy(ar_session);
}

template <>
void inline ScopedGenericArObject<ArFrame*>::Free(ArFrame* ar_frame) {
  ArFrame_destroy(ar_frame);
}

template <>
void inline ScopedGenericArObject<ArConfig*>::Free(ArConfig* ar_config) {
  ArConfig_destroy(ar_config);
}

template <>
void inline ScopedGenericArObject<ArPose*>::Free(ArPose* ar_pose) {
  ArPose_destroy(ar_pose);
}

template <>
void inline ScopedGenericArObject<ArTrackable*>::Free(
    ArTrackable* ar_trackable) {
  ArTrackable_release(ar_trackable);
}

template <>
void inline ScopedGenericArObject<ArPlane*>::Free(ArPlane* ar_plane) {
  // ArPlane itself doesn't have a method to decrease refcount, but it is an
  // instance of ArTrackable & we have to use ArTrackable_release.
  ArTrackable_release(ArAsTrackable(ar_plane));
}

template <>
void inline ScopedGenericArObject<ArAnchor*>::Free(ArAnchor* ar_anchor) {
  ArAnchor_release(ar_anchor);
}

template <>
void inline ScopedGenericArObject<ArTrackableList*>::Free(
    ArTrackableList* ar_trackable_list) {
  ArTrackableList_destroy(ar_trackable_list);
}

template <>
void inline ScopedGenericArObject<ArCamera*>::Free(ArCamera* ar_camera) {
  // Do nothing - ArCamera has no destroy method and is managed by ArCore.
}

template <>
void inline ScopedGenericArObject<ArHitResultList*>::Free(
    ArHitResultList* ar_hit_result_list) {
  ArHitResultList_destroy(ar_hit_result_list);
}

template <>
void inline ScopedGenericArObject<ArHitResult*>::Free(
    ArHitResult* ar_hit_result) {
  ArHitResult_destroy(ar_hit_result);
}

template <class T>
using ScopedArCoreObject = base::ScopedGeneric<T, ScopedGenericArObject<T>>;

}  // namespace internal

using PlaneId = util::IdTypeU64<class PlaneTag>;
using AnchorId = util::IdTypeU64<class AnchorTag>;
using HitTestSubscriptionId = util::IdTypeU64<class HitTestSubscriptionTag>;

struct HitTestSubscriptionData {
  mojom::XRNativeOriginInformationPtr native_origin_information;
  mojom::XRRayPtr ray;

  HitTestSubscriptionData(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      mojom::XRRayPtr ray);
  HitTestSubscriptionData(HitTestSubscriptionData&& other);
  ~HitTestSubscriptionData();
};

// This class should be created and accessed entirely on a Gl thread.
class ArCoreImpl : public ArCore {
 public:
  ArCoreImpl();
  ~ArCoreImpl() override;

  bool Initialize(
      base::android::ScopedJavaLocalRef<jobject> application_context) override;
  void SetDisplayGeometry(const gfx::Size& frame_size,
                          display::Display::Rotation display_rotation) override;
  void SetCameraTexture(GLuint camera_texture_id) override;
  std::vector<float> TransformDisplayUvCoords(
      const base::span<const float> uvs) override;
  gfx::Transform GetProjectionMatrix(float near, float far) override;
  mojom::VRPosePtr Update(bool* camera_updated) override;

  mojom::XRPlaneDetectionDataPtr GetDetectedPlanesData() override;

  mojom::XRAnchorsDataPtr GetAnchorsData() override;

  void Pause() override;
  void Resume() override;

  float GetEstimatedFloorHeight() override;

  bool RequestHitTest(const mojom::XRRayPtr& ray,
                      std::vector<mojom::XRHitResultPtr>* hit_results) override;

  // Helper.
  bool RequestHitTest(const gfx::Point3F& origin,
                      const gfx::Vector3dF& direction,
                      std::vector<mojom::XRHitResultPtr>* hit_results);

  base::Optional<uint64_t> SubscribeToHitTest(
      mojom::XRNativeOriginInformationPtr nativeOriginInformation,
      mojom::XRRayPtr ray) override;

  mojom::XRHitTestSubscriptionResultsDataPtr GetHitTestSubscriptionResults(
      const gfx::Transform& mojo_from_viewer,
      const base::Optional<std::vector<mojom::XRInputSourceStatePtr>>&
          maybe_input_state) override;

  void UnsubscribeFromHitTest(uint64_t subscription_id) override;

  base::Optional<uint64_t> CreateAnchor(
      const device::mojom::PosePtr& pose) override;
  base::Optional<uint64_t> CreateAnchor(const device::mojom::PosePtr& pose,
                                        uint64_t plane_id) override;

  void DetachAnchor(uint64_t anchor_id) override;

 private:
  bool IsOnGlThread();
  base::WeakPtr<ArCoreImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  // An ArCore session, which is distinct and independent of XRSessions.
  // There will only ever be one in Chrome even when supporting
  // multiple XRSessions.
  internal::ScopedArCoreObject<ArSession*> arcore_session_;
  internal::ScopedArCoreObject<ArFrame*> arcore_frame_;

  // List of trackables - used for retrieving planes detected by ARCore. The
  // list will initially be null - call EnsureArCorePlanesList() before using
  // it.
  // Allows reuse of the list across updates; ARCore clears the list on each
  // call to the ARCore SDK.
  internal::ScopedArCoreObject<ArTrackableList*> arcore_planes_;

  // List of anchors - used for retrieving anchors tracked by ARCore. The list
  // will initially be null - call EnsureArCoreAnchorsList() before using it.
  // Allows reuse of the list across updates; ARCore clears the list on each
  // call to the ARCore SDK.
  internal::ScopedArCoreObject<ArAnchorList*> arcore_anchors_;

  // Initializes |arcore_planes_| list.
  void EnsureArCorePlanesList();

  // Initializes |arcore_anchors_| list.
  void EnsureArCoreAnchorsList();

  // Returns vector containing information about all planes updated in current
  // frame, assigning IDs for newly detected planes as needed.
  std::vector<mojom::XRPlaneDataPtr> GetUpdatedPlanesData();

  // This must be called after GetUpdatedPlanesData as it assumes that all
  // planes already have an ID assigned. The result includes freshly assigned
  // IDs for newly detected planes along with previously known IDs for updated
  // and unchanged planes. It excludes planes that are no longer being tracked.
  std::vector<uint64_t> GetAllPlaneIds();

  // Returns vector containing information about all anchors updated in the
  // current frame.
  std::vector<mojom::XRAnchorDataPtr> GetUpdatedAnchorsData();

  // The result will contain IDs of all anchors still tracked in the current
  // frame.
  std::vector<uint64_t> GetAllAnchorIds();

  uint64_t next_id_ = 1;
  std::map<void*, PlaneId> ar_plane_address_to_id_;
  std::map<PlaneId, device::internal::ScopedArCoreObject<ArTrackable*>>
      plane_id_to_plane_object_;
  std::map<void*, AnchorId> ar_anchor_address_to_id_;
  std::map<AnchorId, device::internal::ScopedArCoreObject<ArAnchor*>>
      anchor_id_to_anchor_object_;

  std::map<HitTestSubscriptionId, HitTestSubscriptionData>
      hit_test_subscription_id_to_data_;

  // Returns tuple containing plane id and a boolean signifying that the plane
  // was created.
  std::pair<PlaneId, bool> CreateOrGetPlaneId(void* plane_address);
  std::pair<AnchorId, bool> CreateOrGetAnchorId(void* anchor_address);

  HitTestSubscriptionId CreateHitTestSubscriptionId();

  // Returns hit test subscription results for a single subscription given
  // current XRSpace transformation.
  device::mojom::XRHitTestSubscriptionResultDataPtr
  GetHitTestSubscriptionResult(HitTestSubscriptionId id,
                               const mojom::XRRay& ray,
                               const gfx::Transform& ray_transformation);

  base::Optional<gfx::Transform> GetMojoFromNativeOrigin(
      const mojom::XRNativeOriginInformationPtr& native_origin_information,
      const gfx::Transform& mojo_from_viewer,
      const base::Optional<std::vector<mojom::XRInputSourceStatePtr>>&
          maybe_input_state);

  base::Optional<gfx::Transform> GetMojoFromReferenceSpace(
      device::mojom::XRReferenceSpaceCategory category,
      const gfx::Transform& mojo_from_viewer);

  // Executes |fn| for each still tracked, non-subsumed plane present in
  // |arcore_planes_|.
  template <typename FunctionType>
  void ForEachArCorePlane(FunctionType fn);

  // Executes |fn| for each still tracked anchor present in |arcore_anchors_|.
  template <typename FunctionType>
  void ForEachArCoreAnchor(FunctionType fn);

  // Must be last.
  base::WeakPtrFactory<ArCoreImpl> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ArCoreImpl);
};

class ArCoreImplFactory : public ArCoreFactory {
 public:
  std::unique_ptr<ArCore> Create() override;
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_
