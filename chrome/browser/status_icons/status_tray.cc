// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_tray.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "chrome/browser/status_icons/status_icon.h"

StatusTray::~StatusTray() = default;

StatusIcon* StatusTray::CreateStatusIcon(StatusIconType type,
                                         const gfx::ImageSkia& image,
                                         const std::u16string& tool_tip) {
  auto icon = CreatePlatformStatusIcon(type, image, tool_tip);
  if (!icon)
    return nullptr;

  status_icons_.emplace_back(std::move(icon), type);
  return status_icons_.back().icon.get();
}

std::unique_ptr<StatusIcon> StatusTray::RemoveStatusIcon(StatusIcon* icon) {
  for (auto iter = status_icons_.begin(); iter != status_icons_.end(); ++iter) {
    if (iter->icon.get() == icon) {
      auto removed_icon = std::move(iter->icon);
      status_icons_.erase(iter);
      return removed_icon;
    }
  }
  NOTREACHED();
}

bool StatusTray::HasStatusIconOfTypeForTesting(StatusIconType type) const {
  return std::ranges::any_of(status_icons_,
                             [type](const StatusIconWithType& status_icon) {
                               return status_icon.type == type;
                             });
}

StatusTray::StatusIconWithType::StatusIconWithType(
    std::unique_ptr<StatusIcon> status_icon,
    StatusIconType status_icon_type)
    : icon(std::move(status_icon)), type(status_icon_type) {}
StatusTray::StatusIconWithType::StatusIconWithType(
    StatusIconWithType&& other) noexcept = default;
StatusTray::StatusIconWithType& StatusTray::StatusIconWithType::operator=(
    StatusIconWithType&& other) noexcept = default;
StatusTray::StatusIconWithType::~StatusIconWithType() = default;

StatusTray::StatusTray() = default;
