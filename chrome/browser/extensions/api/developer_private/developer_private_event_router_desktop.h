// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_EVENT_ROUTER_DESKTOP_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_EVENT_ROUTER_DESKTOP_H_

#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_event_router_shared.h"
#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class DeveloperPrivateEventRouter : public DeveloperPrivateEventRouterShared,
                                    public AppWindowRegistry::Observer {
 public:
  explicit DeveloperPrivateEventRouter(Profile* profile);

  DeveloperPrivateEventRouter(const DeveloperPrivateEventRouter&) = delete;
  DeveloperPrivateEventRouter& operator=(const DeveloperPrivateEventRouter&) =
      delete;

  ~DeveloperPrivateEventRouter() override;

 private:
  // AppWindowRegistry::Observer:
  void OnAppWindowAdded(AppWindow* window) override;
  void OnAppWindowRemoved(AppWindow* window) override;

  base::ScopedObservation<AppWindowRegistry, AppWindowRegistry::Observer>
      app_window_registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_EVENT_ROUTER_DESKTOP_H_
