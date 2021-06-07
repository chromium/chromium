// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_GVR_KEYBOARD_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_GVR_KEYBOARD_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/keyboard_ui_interface.h"
#include "third_party/gvr-android-keyboard/src/libraries/headers/vr/gvr/capi/include/gvr_keyboard.h"

namespace vr {

class GvrKeyboardDelegate : public KeyboardDelegate {
 public:
  // Constructs a GvrKeyboardDelegate by dynamically loading the GVR keyboard
  // api. A null pointer is returned upon failure.
  static std::unique_ptr<GvrKeyboardDelegate> Create();
  ~GvrKeyboardDelegate() override;

  typedef int32_t EventType;
  typedef base::RepeatingCallback<void(EventType)> OnEventCallback;

  // KeyboardDelegate implementation.
  void SetUiInterface(KeyboardUiInterface* ui) override;
  void OnBeginFrame() override;
  void ShowKeyboard() override;
  void HideKeyboard() override;
  void SetTransform(const gfx::Transform& transform) override;
  bool HitTest(const gfx::Point3F& ray_origin,
               const gfx::Point3F& ray_target,
               gfx::Point3F* hit_position) override;
  void Draw(const CameraModel& model) override;
  void OnTouchStateUpdated(bool is_touching,
                           const gfx::PointF& touch_position) override;
  bool SupportsSelection() override;
  void OnButtonDown(const gfx::PointF& position) override;
  void OnButtonUp(const gfx::PointF& position) override;
  // Called to update GVR keyboard with the given text input info.
  void UpdateInput(const TextInputInfo& info) override;

 private:
  GvrKeyboardDelegate();
  void Init(gvr_keyboard_context* keyboard_context);
  void OnGvrKeyboardEvent(EventType);
  TextInputInfo GetTextInfo();
  // We pause updates from the keyboard until the previous update has been
  // "acked" using GvrKeyboardDelegate::UpdateInput. This is to prevent weird
  // behavior when editing web input fields. For example, say that the current
  // text is "asdfg" and the user holds the backspace key. We get multiple
  // backspace events from the keyboard, and if the second event comes before
  // first event is acked (can happen because the ack comes from the Renderer),
  // we'll override the keyboard state with the ack.
  // TODO(ymalik): This is brittle, we should look for a better solution.
  bool pause_keyboard_update_ = false;
  TextInputInfo cached_text_input_info_;

  KeyboardUiInterface* ui_;
  gvr_keyboard_context* gvr_keyboard_ = nullptr;
  OnEventCallback keyboard_event_callback_;

  DISALLOW_COPY_AND_ASSIGN(GvrKeyboardDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_GVR_KEYBOARD_DELEGATE_H_
