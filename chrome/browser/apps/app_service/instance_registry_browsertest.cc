// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/app_service/public/cpp/instance.h"
#include "chrome/services/app_service/public/cpp/instance_registry.h"
#include "extensions/common/extension.h"

namespace apps {

using InstanceRegistryBrowserTest = extensions::PlatformAppBrowserTest;

class InstanceRegistryObserver : public apps::InstanceRegistry::Observer {
 public:
  explicit InstanceRegistryObserver(apps::InstanceRegistry* instance_registry)
      : instance_registry_(instance_registry), instance_update_num_(0) {
    Observe(instance_registry);
  }

  ~InstanceRegistryObserver() override = default;

  std::vector<apps::InstanceState>& States() { return states_; }

  int InstanceUpdateNum() { return instance_update_num_; }

 protected:
  // apps::InstanceRegistry::Observer overrides.
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override {
    instance_update_num_++;
    states_.push_back(update.State());
  }

  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* instance_registry) override {
    Observe(nullptr);
  }

  apps::InstanceRegistry* instance_registry_;
  std::vector<apps::InstanceState> states_;
  int instance_update_num_;
};

IN_PROC_BROWSER_TEST_F(InstanceRegistryBrowserTest, ExtensionAppsWindow) {
  if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry)) {
    return;
  }

  AppServiceProxy* app_service_proxy_ =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  ASSERT_TRUE(app_service_proxy_);
  InstanceRegistryObserver observer(&app_service_proxy_->InstanceRegistry());

  const extensions::Extension* app =
      LoadAndLaunchPlatformApp("app_view/host_app", "AppViewTest.LAUNCHED");
  ASSERT_TRUE(app);

  int instance_num = 0;
  InstanceState latest_state = InstanceState::kUnknown;
  app_service_proxy_->InstanceRegistry().ForEachInstance(
      [&instance_num, &latest_state](const apps::InstanceUpdate& inner) {
        instance_num++;
        latest_state = inner.State();
      });

  EXPECT_EQ(1, instance_num);
  EXPECT_NE(0, latest_state | InstanceState::kStarted);
  EXPECT_NE(0, latest_state | InstanceState::kRunning);

  EXPECT_EQ(2, observer.InstanceUpdateNum());
  EXPECT_EQ(2, (int)observer.States().size());
  EXPECT_EQ(InstanceState::kStarted, observer.States()[0]);
  EXPECT_EQ(InstanceState::kStarted | InstanceState::kRunning,
            observer.States()[1]);
}

}  // namespace apps
