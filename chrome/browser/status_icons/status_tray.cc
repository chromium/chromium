// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_tray.h"

#include "chrome/browser/status_icons/status_icon.h"

StatusTray::~StatusTray() {
}

StatusIcon* StatusTray::CreateStatusIcon(StatusIconType type,
                                         const gfx::ImageSkia& image,
                                         const std::u16string& tool_tip) {
  auto icon = CreatePlatformStatusIcon(type, image, tool_tip);
  if (!icon)
    return nullptr;

  status_icons_.push_back(std::move(icon));
  return status_icons_.back().get();
}

void StatusTray::RemoveStatusIcon(StatusIcon* icon) {
  for (auto iter = status_icons_.begin(); iter != status_icons_.end(); ++iter) {
    if (iter->get() == icon) {
      status_icons_.erase(iter);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

StatusTray::StatusTray() {
}
