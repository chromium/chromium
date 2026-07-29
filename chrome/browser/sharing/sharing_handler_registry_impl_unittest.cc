// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_handler_registry_impl.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "components/sharing_message/mock_sharing_message_handler.h"
#include "components/sharing_message/sharing_device_registration.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeSharingDeviceRegistration : public SharingDeviceRegistration {
 public:
  FakeSharingDeviceRegistration() = default;
  ~FakeSharingDeviceRegistration() override = default;

  void RegisterDevice(
      SharingDeviceRegistration::RegistrationCallback callback) override {}
  void UnregisterDevice(
      SharingDeviceRegistration::RegistrationCallback callback) override {}
  bool IsSmsFetcherSupported() const override { return false; }
  bool IsRemoteCopySupported() const override { return false; }
  bool IsOptimizationGuidePushNotificationSupported() const override {
    return false;
  }
  bool IsOneTimeTokenBackendNotificationSupported() const override {
    return false;
  }
  bool IsGlicExperimentalTriggeringSupported() const override { return false; }
  void SetEnabledFeaturesForTesting(
      std::set<syncer::DeviceInfo::SharingFeature> enabled_features) override {}
};

class SharingHandlerRegistryImplTest : public testing::Test {
 public:
  SharingHandlerRegistryImplTest() = default;
  ~SharingHandlerRegistryImplTest() override = default;

  std::unique_ptr<SharingHandlerRegistryImpl> CreateHandlerRegistry() {
    return std::make_unique<SharingHandlerRegistryImpl>(
        /*profile=*/nullptr, &sharing_device_registration_,
        /*message_sender=*/nullptr, /*device_source=*/nullptr,
        /*sms_fetcher=*/nullptr, /*gmail_otp_backend=*/nullptr);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  FakeSharingDeviceRegistration sharing_device_registration_;
};

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
TEST_F(SharingHandlerRegistryImplTest, Glic_NotAddedWhenServiceMissing) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kGlicExperimentalTriggering);
  auto handler_registry = CreateHandlerRegistry();
  EXPECT_FALSE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kGlicExperimentalTriggering));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(SharingHandlerRegistryImplTest, AddRemoveManually) {
  auto handler_registry = CreateHandlerRegistry();
  EXPECT_FALSE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kSmsFetchRequest));

  handler_registry->RegisterSharingHandler(
      std::make_unique<MockSharingMessageHandler>(),
      components_sharing_message::SharingMessage::kSmsFetchRequest);
  EXPECT_TRUE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kSmsFetchRequest));

  handler_registry->UnregisterSharingHandler(
      components_sharing_message::SharingMessage::kSmsFetchRequest);
  EXPECT_FALSE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kSmsFetchRequest));
}
