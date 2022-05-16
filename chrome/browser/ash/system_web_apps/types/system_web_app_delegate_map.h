// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_DELEGATE_MAP_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_DELEGATE_MAP_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"

namespace ash {

using SystemAppDelegateMap =
    base::flat_map<web_app::SystemAppType,
                   std::unique_ptr<web_app::SystemWebAppDelegate>>;

// Returns whether the given app type is enabled.
bool IsSystemWebAppEnabled(const SystemAppDelegateMap& delegates,
                           web_app::SystemAppType type);

// Returns the System App Delegate for the given App |type|.
const web_app::SystemWebAppDelegate* GetSystemWebApp(
    const SystemAppDelegateMap& delegates,
    web_app::SystemAppType type);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_DELEGATE_MAP_H_
