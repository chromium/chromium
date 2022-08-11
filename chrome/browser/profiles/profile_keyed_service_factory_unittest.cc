// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_testing_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

// Testing implementation for interface `ProfileKeyedServiceFactory`.
// The method `GetProfileToUseForTesting()` is used to test the protected method
// `GetBrowserContextToUse()`.
// Default nullptr implementation of `BuildServiceInstanceFor()` since
// `ProfileKeyedServiceFactory` doesn't add any logic to this method.
class ProfileKeyedServiceFactoryTest : public ProfileKeyedServiceFactory {
 public:
  explicit ProfileKeyedServiceFactoryTest(const char* name)
      : ProfileKeyedServiceFactory(name) {}
  ProfileKeyedServiceFactoryTest(const char* name,
                                 const ProfileSelections& selections)
      : ProfileKeyedServiceFactory(name, selections) {}

  // Method used for testing only, calls `GetBrowserContextToUse()` for testing.
  Profile* GetProfileToUseForTesting(Profile* profile) const {
    return Profile::FromBrowserContext(GetBrowserContextToUse(profile));
  }

 protected:
  // Implementation is not for testing.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    NOTREACHED();
    return nullptr;
  }
};

class ProfileKeyedServiceFactoryUnittest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    profile_testing_helper_.SetUp();
  }

 protected:
  void TestProfileToUse(const ProfileKeyedServiceFactoryTest& factory,
                        Profile* given_profile,
                        Profile* expected_profile) {
    EXPECT_EQ(factory.GetProfileToUseForTesting(given_profile),
              expected_profile);
  }

  TestingProfile* regular_profile() {
    return profile_testing_helper_.regular_profile();
  }
  Profile* incognito_profile() {
    return profile_testing_helper_.incognito_profile();
  }

  TestingProfile* guest_profile() {
    return profile_testing_helper_.guest_profile();
  }
  Profile* guest_profile_otr() {
    return profile_testing_helper_.guest_profile_otr();
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestingProfile* system_profile() {
    return profile_testing_helper_.system_profile();
  }
  Profile* system_profile_otr() {
    return profile_testing_helper_.system_profile_otr();
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

 private:
  ProfileTestingHelper profile_testing_helper_;
};

// Factory using default `ProfileKeyedServiceFactory` constructor
class DefaultFactoryTest : public ProfileKeyedServiceFactoryTest {
 public:
  DefaultFactoryTest() : ProfileKeyedServiceFactoryTest("DefaultFactory") {}
};

TEST_F(ProfileKeyedServiceFactoryUnittest, DefaultFactoryTest) {
  DefaultFactoryTest factory;
  TestProfileToUse(factory, regular_profile(), regular_profile());
  TestProfileToUse(factory, incognito_profile(), nullptr);

  TestProfileToUse(factory, guest_profile(), guest_profile());
  TestProfileToUse(factory, guest_profile_otr(), nullptr);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileToUse(factory, system_profile(), system_profile());
  TestProfileToUse(factory, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

// Factory using predefined `ProfileSelections` built
class PredefinedProfileSelectionsFactoryTest
    : public ProfileKeyedServiceFactoryTest {
 public:
  PredefinedProfileSelectionsFactoryTest()
      : ProfileKeyedServiceFactoryTest(
            "PredefinedProfileSelectionsFactoryTest",
            ProfileSelections::BuildRedirectedInIncognito()) {}
};

TEST_F(ProfileKeyedServiceFactoryUnittest,
       PredefinedProfileSelectionsFactoryTest) {
  PredefinedProfileSelectionsFactoryTest factory;
  TestProfileToUse(factory, regular_profile(), regular_profile());
  TestProfileToUse(factory, incognito_profile(), regular_profile());

  TestProfileToUse(factory, guest_profile(), guest_profile());
  TestProfileToUse(factory, guest_profile_otr(), guest_profile());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileToUse(factory, system_profile(), system_profile());
  TestProfileToUse(factory, system_profile_otr(), system_profile());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}

// Factory using customized `ProfileSelections` using
// `ProfileSelections::Builder()`
class CustomizedProfileSelectionsFactoryTest
    : public ProfileKeyedServiceFactoryTest {
 public:
  CustomizedProfileSelectionsFactoryTest()
      : ProfileKeyedServiceFactoryTest(
            "CustomizedProfileSelectionsFactoryTest",
            ProfileSelections::Builder()
                .WithRegular(ProfileSelection::kOriginalOnly)
                .WithGuest(ProfileSelection::kOffTheRecordOnly)
                .WithSystem(ProfileSelection::kNone)
                .Build()) {}
};

TEST_F(ProfileKeyedServiceFactoryUnittest,
       CustomizedProfileSelectionsFactoryTest) {
  CustomizedProfileSelectionsFactoryTest factory;
  TestProfileToUse(factory, regular_profile(), regular_profile());
  TestProfileToUse(factory, incognito_profile(), nullptr);

  TestProfileToUse(factory, guest_profile(), nullptr);
  TestProfileToUse(factory, guest_profile_otr(), guest_profile_otr());

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TestProfileToUse(factory, system_profile(), nullptr);
  TestProfileToUse(factory, system_profile_otr(), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
}
