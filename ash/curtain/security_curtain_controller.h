// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CURTAIN_SECURITY_CURTAIN_CONTROLLER_H_
#define ASH_CURTAIN_SECURITY_CURTAIN_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"

namespace views {
class View;
}  // namespace views

namespace ash::curtain {

using ViewFactory = base::RepeatingCallback<std::unique_ptr<views::View>()>;

// Controller for enabling/disabling the security curtain.
// The security curtain is an overlay that is displayed over all monitors,
// effectively making it impossible for the local user and/or a passerby to see
// what's happening on the ChromeOS device.
// This can for example be used during a Remote Desktop session where the remote
// user wants to ensure their privacy.
class ASH_EXPORT SecurityCurtainController {
 public:
  // Initialization parameters passed to Enable() which allow the caller to
  // tweak the behavior of the security curtain.
  struct InitParams {
    InitParams();
    explicit InitParams(ViewFactory curtain_factory);

    InitParams(const InitParams&);
    InitParams& operator=(const InitParams&);
    InitParams(InitParams&&);
    InitParams& operator=(InitParams&&);
    ~InitParams();

    // Factory that creates the view that will be shown as the curtain overlay.
    // Will be invoked multiple times, once for each monitor.
    ViewFactory curtain_factory;

    // The delay until muting audio output. Can be `base::TimeDelta()` to mute
    // immediately, `base::TimeDelta::Max()` to never mute, or any delay.
    base::TimeDelta mute_audio_output_after;

    bool mute_audio_input = true;
    bool disable_camera_access = true;

    // Disables all input devices (mouse, keyboard, touch, ...) while the
    // security curtain is showing.
    bool disable_input_devices = true;
  };

  virtual ~SecurityCurtainController() = default;

  // Enable the security curtain. This will show a curtain overlay over all
  // displays and block all local user input.
  virtual void Enable(InitParams params) = 0;

  // Disable the security curtain.
  virtual void Disable() = 0;

  // Returns if the security curtain is currently enabled or not.
  virtual bool IsEnabled() const = 0;
};

}  // namespace ash::curtain

#endif  // ASH_CURTAIN_SECURITY_CURTAIN_CONTROLLER_H_
