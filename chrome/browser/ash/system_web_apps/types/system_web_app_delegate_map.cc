// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate_map.h"

#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"

namespace ash {

bool IsSystemWebAppEnabled(const SystemWebAppDelegateMap& delegates,
                           SystemWebAppType type) {
  if (base::FeatureList::IsEnabled(features::kEnableAllSystemWebApps))
    return true;

  const SystemWebAppDelegate* delegate = GetSystemWebApp(delegates, type);
  if (!delegate)
    return false;

  return delegate->IsAppEnabled();
}

const SystemWebAppDelegate* GetSystemWebApp(
    const SystemWebAppDelegateMap& delegates,
    SystemWebAppType type) {
  auto it = delegates.find(type);
  if (it == delegates.end())
    return nullptr;

  return it->second.get();
}

}  // namespace ash
