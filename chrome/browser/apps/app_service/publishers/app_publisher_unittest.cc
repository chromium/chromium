// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/app_publisher.h"

#include <memory>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

class TestPublisher : public AppPublisher {
 public:
  explicit TestPublisher(AppServiceProxy* proxy, AppType app_type)
      : AppPublisher(proxy), app_type_(app_type) {
    RegisterPublisher(app_type);
  }

  void MakeAndPublishApp(const std::string& app_id,
                         bool initial_camera,
                         bool initial_microphone) {
    AppPublisher::Publish(
        AppPublisher::MakeApp(app_type_, app_id, Readiness::kReady, app_id,
                              InstallReason::kUser, InstallSource::kPlayStore));

    ModifyCapabilityAccess(app_id, initial_camera, initial_microphone);
  }

  // AppPublisher:
  void LoadIcon(const std::string& app_id,
                const IconKey& icon_key,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override {}
  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override {}
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override {}

  // These methods are private in AppPublisher, we expose them here for unit
  // testing.
  void Publish(std::vector<AppPtr> apps,
               bool should_notify_initialized = false) {
    AppPublisher::Publish(std::move(apps), app_type_,
                          should_notify_initialized);
  }

  void ResetCapabilityAccess() {
    AppPublisher::ResetCapabilityAccess(app_type_);
  }

 private:
  AppType app_type_;
};

}  // namespace

class AppPublisherTest : public testing::Test {
 public:
  void SetUp() override {
    app_service_test_.SetUp(&profile_);
    proxy_ = app_service_test_.proxy();
    publisher_ = std::make_unique<TestPublisher>(proxy_, AppType::kArc);

    publisher_->Publish({}, /*should_notify_initialized=*/true);
  }

  TestPublisher* publisher() { return publisher_.get(); }

  AppServiceProxy* proxy() { return proxy_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  raw_ptr<AppServiceProxy> proxy_;
  std::unique_ptr<TestPublisher> publisher_;
  AppServiceTest app_service_test_;
};

TEST_F(AppPublisherTest, ModifyCapabilityAccess_PublishesToCapabilityCache) {
  publisher()->MakeAndPublishApp("a", /*initial_camera=*/true,
                                 /*initial_microphone=*/false);
  publisher()->MakeAndPublishApp("b", /*initial_camera=*/false,
                                 /*initial_microphone=*/true);

  ASSERT_THAT(proxy()->AppCapabilityAccessCache().GetAppsAccessingCamera(),
              testing::ElementsAre("a"));
  ASSERT_THAT(proxy()->AppCapabilityAccessCache().GetAppsAccessingMicrophone(),
              testing::ElementsAre("b"));
}

// Verifies that ResetCapabilityAccess() removes all apps of the type from
// AppCapabilityAccessCache, but does not modify other app types.
TEST_F(AppPublisherTest, ResetCapabilityAccess_ResetsAppsOfType) {
  publisher()->MakeAndPublishApp("a", /*initial_camera=*/true,
                                 /*initial_microphone=*/false);
  publisher()->MakeAndPublishApp("b", /*initial_camera=*/false,
                                 /*initial_microphone=*/true);

  // Publish another app with a different type.
  TestPublisher publisher_web(proxy(), AppType::kWeb);
  publisher_web.MakeAndPublishApp("c", /*initial_camera=*/true,
                                  /*initial_microphone=*/true);

  publisher()->ResetCapabilityAccess();

  ASSERT_THAT(
      proxy()->AppCapabilityAccessCache().GetAppsAccessingCapabilities(),
      testing::ElementsAre("c"));
}

}  // namespace apps
