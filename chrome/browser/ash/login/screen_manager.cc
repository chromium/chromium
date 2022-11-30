// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screen_manager.h"

#include <iostream>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

ScreenManager::ScreenManager() = default;

ScreenManager::~ScreenManager() = default;

void ScreenManager::Init(
    std::vector<std::pair<OobeScreenId, std::unique_ptr<BaseScreen>>> screens) {
  screens_ = decltype(screens_)(std::move(screens));
}

BaseScreen* ScreenManager::GetScreen(OobeScreenId screen) {
  auto iter = screens_.find(screen);
  CHECK(iter != screens_.end()) << "Failed to find screen " << screen;
  return iter->second.get();
}

OobeScreenId ScreenManager::GetScreenByName(const std::string& screen_name) {
  OobeScreenId screen = OobeScreenId(screen_name);
  auto iter = screens_.find(screen);
  CHECK(iter != screens_.end()) << "Failed to find screen " << screen;
  return iter->first;
}

bool ScreenManager::HasScreen(OobeScreenId screen) {
  return screens_.count(screen) > 0;
}

void ScreenManager::SetScreenForTesting(std::unique_ptr<BaseScreen> value) {
  // Capture screen id to avoid using `value` after moving it; = is not a
  // sequence point.
  auto id = value->screen_id();
  screens_[id] = std::move(value);
}

void ScreenManager::DeleteScreenForTesting(OobeScreenId screen) {
  screens_[screen] = nullptr;
}

void ScreenManager::Shutdown() {
  screens_.clear();
}

}  // namespace ash
