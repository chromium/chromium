// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_test_util.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile_window.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "testing/gtest/include/gtest/gtest.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace profiles::testing {

Profile* CreateProfileSync(ProfileManager* profile_manager,
                           const base::FilePath& path) {
  base::test::TestFuture<Profile*> profile_future;
  profile_manager->CreateProfileAsync(path, profile_future.GetCallback());
  Profile* profile = profile_future.Get();
  CHECK(profile);
  return profile;
}

#if !BUILDFLAG(IS_ANDROID)

void SwitchToProfileSync(const base::FilePath& path, bool always_create) {
  base::test::TestFuture<Profile*> future;
  profiles::SwitchToProfile(path, always_create, future.GetCallback());
  ASSERT_TRUE(future.Wait()) << "profiles::SwitchToProfile() did not complete";
}

ScopedNonEnterpriseDomainSetterForTesting::
    ScopedNonEnterpriseDomainSetterForTesting(const char* domain) {
  policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(domain);
}

ScopedNonEnterpriseDomainSetterForTesting::
    ~ScopedNonEnterpriseDomainSetterForTesting() {
  policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(nullptr);
}

#endif  // !BUILDFLAG(IS_ANDROID)

ScopedProfileSelectionsForFactoryTesting::
    ScopedProfileSelectionsForFactoryTesting(
        ProfileKeyedServiceFactory* factory,
        ProfileSelections selections)
    : factory_(factory),
      old_selections_(std::exchange(factory->profile_selections_, selections)) {
}

ScopedProfileSelectionsForFactoryTesting::
    ~ScopedProfileSelectionsForFactoryTesting() {
  factory_->profile_selections_ = old_selections_;
}

}  // namespace profiles::testing
