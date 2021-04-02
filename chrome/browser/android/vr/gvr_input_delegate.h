// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_GVR_INPUT_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_GVR_INPUT_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/android/vr/vr_controller.h"
#include "chrome/browser/vr/input_delegate.h"

namespace gvr {
class GvrApi;
}

namespace vr {

class GestureDetector;

class GvrInputDelegate : public InputDelegate {
 public:
  explicit GvrInputDelegate(gvr::GvrApi* gvr_api);
  ~GvrInputDelegate() override;

  // InputDelegate implementation.
  void OnTriggerEvent(bool pressed) override;
  gfx::Transform GetHeadPose() override;
  void UpdateController(const gfx::Transform& head_pose,
                        base::TimeTicks current_time,
                        bool is_webxr_frame) override;
  ControllerModel GetControllerModel(const gfx::Transform& head_pose) override;
  InputEventList GetGestures(base::TimeTicks current_time) override;
  device::mojom::XRInputSourceStatePtr GetInputSourceState() override;
  void OnResume() override;
  void OnPause() override;

 private:
  std::unique_ptr<VrController> controller_;
  GestureDetector gesture_detector_;
  gvr::GvrApi* gvr_api_;

  bool was_select_button_down_ = false;

  DISALLOW_COPY_AND_ASSIGN(GvrInputDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_GVR_INPUT_DELEGATE_H_
