// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_H_

#include <memory>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/display/display.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_bindings.h"

namespace device {

// This allows a real or fake implementation of ArCore to
// be used as appropriate (i.e. for testing).
class ArCore {
 public:
  virtual ~ArCore() = default;

  // Initializes the runtime and returns whether it was successful.
  // If successful, the runtime must be paused when this method returns.
  virtual bool Initialize(
      base::android::ScopedJavaLocalRef<jobject> application_context) = 0;

  virtual void SetDisplayGeometry(
      const gfx::Size& frame_size,
      display::Display::Rotation display_rotation) = 0;
  virtual void SetCameraTexture(GLuint camera_texture_id) = 0;
  // Transform the given UV coordinates by the current display rotation.
  virtual std::vector<float> TransformDisplayUvCoords(
      const base::span<const float> uvs) = 0;
  virtual gfx::Transform GetProjectionMatrix(float near, float far) = 0;

  // Update ArCore state. This call blocks for up to 1/30s while waiting for a
  // new camera image. The output parameter |camera_updated| must be non-null,
  // the stored value indicates if the camera image was updated successfully.
  // The returned pose is nullptr if tracking was lost, this can happen even
  // when the camera image was updated successfully.
  virtual mojom::VRPosePtr Update(bool* camera_updated) = 0;

  // Return latest estimate for the floor height.
  virtual float GetEstimatedFloorHeight() = 0;

  // Returns information about all planes detected in the current frame.
  virtual mojom::XRPlaneDetectionDataPtr GetDetectedPlanesData() = 0;

  // Returns information about all anchors tracked in the current frame.
  virtual mojom::XRAnchorsDataPtr GetAnchorsData() = 0;

  virtual bool RequestHitTest(
      const mojom::XRRayPtr& ray,
      std::vector<mojom::XRHitResultPtr>* hit_results) = 0;

  virtual base::Optional<uint64_t> SubscribeToHitTest(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      mojom::XRRayPtr ray) = 0;

  virtual mojom::XRHitTestSubscriptionResultsDataPtr
  GetHitTestSubscriptionResults(
      const gfx::Transform& mojo_from_viewer,
      const base::Optional<std::vector<mojom::XRInputSourceStatePtr>>&
          maybe_input_state) = 0;

  virtual void UnsubscribeFromHitTest(uint64_t subscription_id) = 0;

  virtual base::Optional<uint64_t> CreateAnchor(const mojom::PosePtr& pose) = 0;
  virtual base::Optional<uint64_t> CreateAnchor(const mojom::PosePtr& pose,
                                                uint64_t plane_id) = 0;

  virtual void DetachAnchor(uint64_t anchor_id) = 0;

  virtual void Pause() = 0;
  virtual void Resume() = 0;
};

class ArCoreFactory {
 public:
  virtual ~ArCoreFactory() = default;
  virtual std::unique_ptr<ArCore> Create() = 0;
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_H_
