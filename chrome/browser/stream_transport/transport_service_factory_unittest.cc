// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/stream_transport/transport_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_actuator/public/features.h"
#include "components/stream_transport/public/transport_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace stream_transport {
namespace {

class TransportServiceFactoryTest : public testing::Test {
 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(TransportServiceFactoryTest, CreatesServiceForRegularProfile) {
  feature_list_.InitAndEnableFeature(browser_actuator::kBrowserActuator);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  EXPECT_NE(TransportServiceFactory::GetForProfile(profile.get()), nullptr);
}

TEST_F(TransportServiceFactoryTest, ReturnsNullWhenBrowserActuatorIsDisabled) {
  feature_list_.InitAndDisableFeature(browser_actuator::kBrowserActuator);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  EXPECT_EQ(TransportServiceFactory::GetForProfile(profile.get()), nullptr);
}

TEST_F(TransportServiceFactoryTest, ReturnsNullForOffTheRecordProfile) {
  feature_list_.InitAndEnableFeature(browser_actuator::kBrowserActuator);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  Profile* off_the_record_profile = profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);

  EXPECT_EQ(TransportServiceFactory::GetForProfile(off_the_record_profile),
            nullptr);
}

TEST_F(TransportServiceFactoryTest, ReturnsNullForGuestProfile) {
  feature_list_.InitAndEnableFeature(browser_actuator::kBrowserActuator);
  std::unique_ptr<TestingProfile> profile =
      TestingProfile::Builder().SetGuestSession().Build();

  EXPECT_EQ(TransportServiceFactory::GetForProfile(profile.get()), nullptr);
}

TEST_F(TransportServiceFactoryTest, RecordsSessionsWhenInternalsPageIsEnabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{browser_actuator::kBrowserActuator,
                            browser_actuator::kBrowserActuatorInternals},
      /*disabled_features=*/{});
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  TransportService* service =
      TransportServiceFactory::GetForProfile(profile.get());
  ASSERT_NE(service, nullptr);
  EXPECT_NE(service->GetStreamRecorderFactory(), nullptr)
      << "kBrowserActuatorInternals should turn session recording on.";
}

TEST_F(TransportServiceFactoryTest,
       DoesNotRecordSessionsWhenInternalsPageIsDisabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{browser_actuator::kBrowserActuator},
      /*disabled_features=*/{browser_actuator::kBrowserActuatorInternals});
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  TransportService* service =
      TransportServiceFactory::GetForProfile(profile.get());
  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetStreamRecorderFactory(), nullptr)
      << "Recording costs memory and should be off without the internals page.";
}

}  // namespace
}  // namespace stream_transport
