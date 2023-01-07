// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_CARDBOARD_INPUT_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_CARDBOARD_INPUT_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/input_delegate.h"

namespace gvr {
class GvrApi;
}

namespace vr {

class CardboardInputDelegate : public InputDelegate {
 public:
  explicit CardboardInputDelegate(gvr::GvrApi* gvr_api);

  CardboardInputDelegate(const CardboardInputDelegate&) = delete;
  CardboardInputDelegate& operator=(const CardboardInputDelegate&) = delete;

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
  raw_ptr<gvr::GvrApi> gvr_api_;
  bool cardboard_trigger_pressed_ = false;
  bool cardboard_trigger_clicked_ = false;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_CARDBOARD_INPUT_DELEGATE_H_
