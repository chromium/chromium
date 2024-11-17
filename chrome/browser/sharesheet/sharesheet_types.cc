// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_types.h"

namespace sharesheet {

TargetInfo::TargetInfo(
    TargetType type,
    std::optional<gfx::ImageSkia> icon,
    const std::u16string& launch_name,
    const std::u16string& display_name,
    const std::optional<ShareActionType>& share_action_type,
    const std::optional<std::u16string>& secondary_display_name,
    const std::optional<std::string>& activity_name,
    bool is_dlp_blocked)
    : type(type),
      icon(std::move(icon)),
      launch_name(launch_name),
      display_name(display_name),
      share_action_type(share_action_type),
      secondary_display_name(secondary_display_name),
      activity_name(activity_name),
      is_dlp_blocked(is_dlp_blocked) {}

TargetInfo::~TargetInfo() = default;

TargetInfo::TargetInfo(TargetInfo&& other) = default;

TargetInfo& TargetInfo::operator=(TargetInfo&& other) = default;

TargetInfo::TargetInfo(const TargetInfo&) = default;

TargetInfo& TargetInfo::operator=(const TargetInfo&) = default;

}  // namespace sharesheet
