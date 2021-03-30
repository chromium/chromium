// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_types.h"

namespace sharesheet {

TargetInfo::TargetInfo(
    TargetType type,
    const base::Optional<gfx::ImageSkia> icon,
    const std::u16string& launch_name,
    const std::u16string& display_name,
    const base::Optional<std::u16string>& secondary_display_name,
    const base::Optional<std::string>& activity_name)
    : type(type),
      icon(icon),
      launch_name(launch_name),
      display_name(display_name),
      secondary_display_name(secondary_display_name),
      activity_name(activity_name) {}

TargetInfo::~TargetInfo() = default;

TargetInfo::TargetInfo(TargetInfo&& other) = default;

TargetInfo& TargetInfo::operator=(TargetInfo&& other) = default;

}  // namespace sharesheet
