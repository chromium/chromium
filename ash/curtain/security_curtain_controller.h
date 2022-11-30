// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CURTAIN_SECURITY_CURTAIN_CONTROLLER_H_
#define ASH_CURTAIN_SECURITY_CURTAIN_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/callback.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash::curtain {

enum class FilterResult { kKeepEvent, kSuppressEvent };
using EventFilter = base::RepeatingCallback<FilterResult(const ui::Event&)>;

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
    explicit InitParams(EventFilter filter);

    InitParams(const InitParams&);
    InitParams& operator=(const InitParams&);
    InitParams(InitParams&&);
    InitParams& operator=(InitParams&&);
    ~InitParams();

    // Filter to specify which input |ui::Event|s should or should not be
    // suppressed. If unspecified all input events will be suppressed.
    EventFilter event_filter;
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
