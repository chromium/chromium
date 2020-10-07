// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SCOPED_LIGHT_MODE_AS_DEFAULT_H_
#define ASH_STYLE_SCOPED_LIGHT_MODE_AS_DEFAULT_H_

namespace ash {

// A helper class to set default color mode to light. Color mode is dark by
// default, which is controlled by pref |kDarkModeEnabled| currently. Some of
// the components need to be kept as LIGHT by default before launching
// dark/light mode. E.g. power button menu and system toast. Overriding only if
// the kDarkLightMode feature is disabled.
class ScopedLightModeAsDefault {
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

#endif  // ASH_STYLE_SCOPED_LIGHT_MODE_AS_DEFAULT_H_
