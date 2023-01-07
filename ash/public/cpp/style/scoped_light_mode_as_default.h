// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_STYLE_SCOPED_LIGHT_MODE_AS_DEFAULT_H_
#define ASH_PUBLIC_CPP_STYLE_SCOPED_LIGHT_MODE_AS_DEFAULT_H_

#include "ash/ash_export.h"

namespace ash {

// A helper class to set color mode to LIGHT when the DarkLightMode feature is
// disabled. Color mode is DARK by default when it is disabled currently. Some
// of the components need to be kept as LIGHT by default before launching the
// feature. E.g, power button menu and system toast. Overriding only if the
// DarkLightMode feature is disabled.
class ASH_EXPORT ScopedLightModeAsDefault {
 public:
  ScopedLightModeAsDefault();
  ScopedLightModeAsDefault(const ScopedLightModeAsDefault&) = delete;
  ScopedLightModeAsDefault& operator=(const ScopedLightModeAsDefault&) = delete;
  ~ScopedLightModeAsDefault();

 private:
  // Value of |override_light_mode_as_default_| inside AshColorProvider before
  // setting.
  bool previous_override_light_mode_as_default_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_STYLE_SCOPED_LIGHT_MODE_AS_DEFAULT_H_
