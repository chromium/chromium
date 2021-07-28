// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_SUBSCRIBER_CROSAPI_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_SUBSCRIBER_CROSAPI_H_

#include <memory>
#include <string>
#include <vector>

#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"

class Profile;

namespace apps {

// App service subscriber to support App Service Proxy in Lacros.
// This object is used as a proxy to communicate between the
// crosapi and App Service.
//
// See components/services/app_service/README.md.
class SubscriberCrosapi : public KeyedService, public apps::mojom::Subscriber {
 public:
  explicit SubscriberCrosapi(Profile* profile);
  SubscriberCrosapi(const SubscriberCrosapi&) = delete;
  SubscriberCrosapi& operator=(const SubscriberCrosapi&) = delete;
  ~SubscriberCrosapi() override;

 protected:
  // apps::mojom::Subscriber overrides.
  void OnApps(std::vector<apps::mojom::AppPtr> deltas,
              apps::mojom::AppType app_type,
              bool should_notify_initialized) override;
  void OnCapabilityAccesses(
      std::vector<apps::mojom::CapabilityAccessPtr> deltas) override;
  void Clone(mojo::PendingReceiver<apps::mojom::Subscriber> receiver) override;
  void OnPreferredAppSet(const std::string& app_id,
                         apps::mojom::IntentFilterPtr intent_filter) override;
  void OnPreferredAppRemoved(
      const std::string& app_id,
      apps::mojom::IntentFilterPtr intent_filter) override;
  void InitializePreferredApps(
      PreferredAppsList::PreferredApps preferred_apps) override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_SUBSCRIBER_CROSAPI_H_
