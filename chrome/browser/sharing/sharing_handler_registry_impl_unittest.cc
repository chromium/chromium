// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_handler_registry_impl.h"

#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browser_actuator/public/features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sharing_message/mock_sharing_message_handler.h"
#include "components/sharing_message/sharing_device_registration.h"
#include "content/public/browser/browser_context.h"
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
  bool IsBrowserActuatorSupported() const override { return false; }
  void SetEnabledFeaturesForTesting(
      std::set<syncer::DeviceInfo::SharingFeature> enabled_features) override {}
};

class SharingHandlerRegistryImplTest : public testing::Test {
 public:
  SharingHandlerRegistryImplTest() = default;
  ~SharingHandlerRegistryImplTest() override = default;

  std::unique_ptr<SharingHandlerRegistryImpl> CreateHandlerRegistry(
      Profile* profile = nullptr) {
    return std::make_unique<SharingHandlerRegistryImpl>(
        profile, &sharing_device_registration_,
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

TEST_F(SharingHandlerRegistryImplTest,
       Glic_AddedEvenWhenBrowserActuatorEnabled) {
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kGlicExperimentalTriggering,
       browser_actuator::kBrowserActuator,
       browser_actuator::kEnableBrowserActuatorForGlicExperimentalTriggering},
      {});
  glic::GlicProfileManager glic_profile_manager;
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  TestingProfile::TestingFactories testing_factories;
  testing_factories.emplace_back(
      glic::GlicKeyedServiceFactory::GetInstance(),
      base::BindLambdaForTesting([&](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
        Profile* profile = Profile::FromBrowserContext(context);
        return std::make_unique<glic::GlicKeyedService>(
            profile, IdentityManagerFactory::GetForProfile(profile),
            profile_manager.profile_manager(), &glic_profile_manager,
            /*contextual_cueing_service=*/nullptr,
            /*actor_keyed_service=*/nullptr);
      }));
  TestingProfile* profile = profile_manager.CreateTestingProfile(
      "profile", std::move(testing_factories));
  auto handler_registry = CreateHandlerRegistry(profile);
  EXPECT_TRUE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kGlicExperimentalTriggering));
  EXPECT_TRUE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kActuatorDownstreamMessage));
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

TEST_F(SharingHandlerRegistryImplTest,
       BrowserActuatorDownstreamMessage_AddedWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(browser_actuator::kBrowserActuator);
  auto handler_registry = CreateHandlerRegistry();
  EXPECT_TRUE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kActuatorDownstreamMessage));
}

TEST_F(SharingHandlerRegistryImplTest,
       BrowserActuatorDownstreamMessage_NotAddedWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(browser_actuator::kBrowserActuator);
  auto handler_registry = CreateHandlerRegistry();
  EXPECT_FALSE(handler_registry->GetSharingHandler(
      components_sharing_message::SharingMessage::kActuatorDownstreamMessage));
}
