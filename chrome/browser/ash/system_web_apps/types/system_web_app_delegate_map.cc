// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate_map.h"

namespace ash {

const SystemWebAppDelegate* GetSystemWebApp(
    const SystemWebAppDelegateMap& delegates,
    SystemWebAppType type) {
  auto it = delegates.find(type);
  if (it == delegates.end())
    return nullptr;

  return it->second.get();
}

}  // namespace ash
