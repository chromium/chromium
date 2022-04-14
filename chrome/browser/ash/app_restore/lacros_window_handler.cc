// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/lacros_window_handler.h"

#include "chrome/browser/apps/app_service/browser_app_instance.h"
#include "components/app_restore/app_restore_utils.h"

namespace ash {
namespace app_restore {

LacrosWindowHandler::LacrosWindowHandler(
    apps::BrowserAppInstanceRegistry& browser_app_instance_registry) {
  browser_app_instance_registry_observation_.Observe(
      &browser_app_instance_registry);
}

LacrosWindowHandler::~LacrosWindowHandler() = default;

void LacrosWindowHandler::OnBrowserWindowAdded(
    const apps::BrowserWindowInstance& instance) {
  ::app_restore::OnLacrosWindowAdded(
      instance.window, instance.browser_session_id,
      instance.restored_browser_session_id, /*is_browser_app=*/false);
}

void LacrosWindowHandler::OnBrowserAppAdded(
    const apps::BrowserAppInstance& instance) {
  if (instance.type == apps::BrowserAppInstance::Type::kAppWindow) {
    ::app_restore::OnLacrosWindowAdded(
        instance.window, instance.browser_session_id,
        instance.restored_browser_session_id, /*is_browser_app=*/true);
  }
}

}  // namespace app_restore
}  // namespace ash
