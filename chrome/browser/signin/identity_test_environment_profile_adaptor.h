// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_IDENTITY_TEST_ENVIRONMENT_PROFILE_ADAPTOR_H_
#define CHROME_BROWSER_SIGNIN_IDENTITY_TEST_ENVIRONMENT_PROFILE_ADAPTOR_H_

#include <string>

#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

// Adaptor that supports signin::IdentityTestEnvironment's usage in testing
// contexts where the relevant fake objects must be injected via the
// BrowserContextKeyedServiceFactory infrastructure as the production code
// accesses IdentityManager via that infrastructure. Before using this
// class, please consider whether you can change the production code in question
// to take in the relevant dependencies directly rather than obtaining them from
// the Profile; this is both cleaner in general and allows for direct usage of
// signin::IdentityTestEnvironment in the test.
class IdentityTestEnvironmentProfileAdaptor {
 public:
  // Creates and returns a TestingProfile that has been configured with the set
  // of testing factories that IdentityTestEnvironment requires.
  static std::unique_ptr<TestingProfile>
  CreateProfileForIdentityTestEnvironment();

  // Like the above, but additionally configures the returned Profile with
  // |input_factories|.
  static std::unique_ptr<TestingProfile>
  CreateProfileForIdentityTestEnvironment(
      const TestingProfile::TestingFactories& input_factories);

  // Creates and returns a TestingProfile that has been configured with the
  // given |builder| and the set of testing factories that
  // IdentityTestEnvironment requires.
  // See the above variant for comments on common parameters.
  static std::unique_ptr<TestingProfile>
  CreateProfileForIdentityTestEnvironment(TestingProfile::Builder& builder);

  // Sets the testing factories that signin::IdentityTestEnvironment
  // requires explicitly on a Profile that is passed to it.
  // See the above variant for comments on common parameters.
  static void SetIdentityTestEnvironmentFactoriesOnBrowserContext(
      content::BrowserContext* browser_context);

  // Appends the set of testing factories that signin::IdentityTestEnvironment
  // requires to |factories_to_append_to|, which should be the set of testing
  // factories supplied to TestingProfile (via one of the various mechanisms for
  // doing so). Prefer the above API if possible, as it is less fragile. This
  // API is primarily for use in tests that do not create the TestingProfile
  // internally but rather simply supply the set of TestingFactories to some
  // external facility (e.g., a superclass).
  // See CreateProfileForIdentityTestEnvironment() for comments on common
  // parameters.
  static void AppendIdentityTestEnvironmentFactories(
      TestingProfile::TestingFactories* factories_to_append_to);

  // Returns the set of testing factories that signin::IdentityTestEnvironment
  // requires, which can be useful to configure profiles for services that do
  // not require any other testing factory than the ones specified in here.
  static TestingProfile::TestingFactories GetIdentityTestEnvironmentFactories();

  // Constructs an adaptor that associates an IdentityTestEnvironment instance
  // with |profile| via the relevant backing objects. Note that
  // |profile| must have been configured with the IdentityTestEnvironment
  // testing factories, either because it was created via
  // CreateProfileForIdentityTestEnvironment() or because
  // AppendIdentityTestEnvironmentFactories() was invoked on the set of
  // factories supplied to it.
  // |profile| must outlive this object.
  explicit IdentityTestEnvironmentProfileAdaptor(Profile* profile);
  ~IdentityTestEnvironmentProfileAdaptor() {}

  // Returns the IdentityTestEnvironment associated with this object (and
  // implicitly with the Profile passed to this object's constructor).
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  // Testing factory that creates an IdentityManager
  // with a FakeProfileOAuth2TokenService.
  static std::unique_ptr<KeyedService> BuildIdentityManagerForTests(
      content::BrowserContext* context);

  signin::IdentityTestEnvironment identity_test_env_;

  DISALLOW_COPY_AND_ASSIGN(IdentityTestEnvironmentProfileAdaptor);
};

#endif  // CHROME_BROWSER_SIGNIN_IDENTITY_TEST_ENVIRONMENT_PROFILE_ADAPTOR_H_
