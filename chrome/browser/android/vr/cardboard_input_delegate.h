// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_CARDBOARD_INPUT_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_CARDBOARD_INPUT_DELEGATE_H_

#include "base/macros.h"
#include "chrome/browser/vr/input_delegate.h"

namespace gvr {
class GvrApi;
}

namespace vr {

class CardboardInputDelegate : public InputDelegate {
 public:
  explicit CardboardInputDelegate(gvr::GvrApi* gvr_api);
  ~CardboardInputDelegate() override;

  // InputDelegate implementation.
  gfx::Transform GetHeadPose() override;
  void OnTriggerEvent(bool pressed) override;
  void UpdateController(const gfx::Transform& head_pose,
                        base::TimeTicks current_time,
                        bool is_webxr_frame) override;
  ControllerModel GetControllerModel(const gfx::Transform& head_pose) override;
  InputEventList GetGestures(base::TimeTicks current_time) override;
  device::mojom::XRInputSourceStatePtr GetInputSourceState() override;
  void OnResume() override;
  void OnPause() override;

 private:
  gvr::GvrApi* gvr_api_;
  bool cardboard_trigger_pressed_ = false;
  bool cardboard_trigger_clicked_ = false;

  DISALLOW_COPY_AND_ASSIGN(CardboardInputDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_CARDBOARD_INPUT_DELEGATE_H_
