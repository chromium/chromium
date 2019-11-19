// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_manager.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

ScreenManager::ScreenManager() = default;

ScreenManager::~ScreenManager() = default;

void ScreenManager::Init(std::vector<std::unique_ptr<BaseScreen>> screens) {
  for (auto&& screen : screens)
    screens_[screen->screen_id()] = std::move(screen);
}

BaseScreen* ScreenManager::GetScreen(OobeScreenId screen) {
  auto iter = screens_.find(screen);
  DCHECK(iter != screens_.end()) << "Failed to find screen " << screen;
  return iter->second.get();
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

}  // namespace chromeos
