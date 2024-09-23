// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_core_host_factory.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ip_protection/ip_protection_core_host.h"
#include "chrome/browser/ip_protection/ip_protection_switches.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class ScopedInitFeature {
 public:
  ScopedInitFeature(const base::Feature& feature, bool enable) {
    feature_list_.InitWithFeatureState(feature, enable);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ScopedInitCommandLine {
 public:
  explicit ScopedInitCommandLine(std::string_view command_line_switch) {
    if (!command_line_switch.empty()) {
      command_line_.GetProcessCommandLine()->AppendSwitch(command_line_switch);
    }
  }

 private:
  base::test::ScopedCommandLine command_line_;
};
}  // namespace

class IpProtectionCoreHostFactoryTest : public testing::Test {
 protected:
  explicit IpProtectionCoreHostFactoryTest(bool feature_enabled = true,
                                           const char* command_line_switch = "")
      // Note that the order of initialization is important here - we want to
      // set the value of the feature before anything else since it's used by
      // the `IpProtectionCoreHostFactory` logic. Same for the command
      // line switch, if specified.
      : scoped_feature_(net::features::kEnableIpProtectionProxy,
                        feature_enabled),
        scoped_command_line_(command_line_switch),
        profile_selections_(
            IpProtectionCoreHostFactory::GetInstance(),
            IpProtectionCoreHostFactory::CreateProfileSelectionsForTesting()) {}

  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
  }

  void TearDown() override { profile_.reset(); }

  TestingProfile* profile() { return profile_.get(); }

  ScopedInitFeature scoped_feature_;
  ScopedInitCommandLine scoped_command_line_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      profile_selections_;
  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(IpProtectionCoreHostFactoryTest,
       ServiceCreationSucceedsWhenFlagEnabled) {
  IpProtectionCoreHost* service =
      IpProtectionCoreHostFactory::GetForProfile(profile());
  ASSERT_TRUE(service);
  service->Shutdown();
}

TEST_F(IpProtectionCoreHostFactoryTest, OtrProfileUsesPrimaryProfile) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // The regular profile and the off-the-record profile must be different.
  ASSERT_NE(profile(), otr_profile);

  // The same `IpProtectionCoreHost` should be used for both the main
  // profile and the corresponding OTR profile.
  EXPECT_EQ(IpProtectionCoreHostFactory::GetForProfile(profile()),
            IpProtectionCoreHostFactory::GetForProfile(otr_profile));
}

class IpProtectionCoreHostFactoryFeatureDisabledTest
    : public IpProtectionCoreHostFactoryTest {
 public:
  IpProtectionCoreHostFactoryFeatureDisabledTest()
      : IpProtectionCoreHostFactoryTest(/*feature_enabled=*/false) {}
};

TEST_F(IpProtectionCoreHostFactoryFeatureDisabledTest,
       ServiceCreationFailsWhenFlagDisabled) {
  IpProtectionCoreHost* service =
      IpProtectionCoreHostFactory::GetForProfile(profile());
  ASSERT_FALSE(service);
}

class IpProtectionCoreHostFactoryOptOutEnabled
    : public IpProtectionCoreHostFactoryTest {
 public:
  IpProtectionCoreHostFactoryOptOutEnabled()
      : IpProtectionCoreHostFactoryTest(
            /*feature_enabled=*/true,
            /*command_line_switch=*/switches::kDisableIpProtectionProxy) {}
};

TEST_F(IpProtectionCoreHostFactoryOptOutEnabled,
       ServiceCreationFailsWhenUserOptedOut) {
  IpProtectionCoreHost* service =
      IpProtectionCoreHostFactory::GetForProfile(profile());
  ASSERT_FALSE(service);
}
