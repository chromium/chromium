// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/user_manager/multi_user/multi_user_sign_in_policy_controller.h"

// TODO(b/278643115): Move to components/user_manager/multi_user
// once we remove the dependency to chrome/*

#include <stddef.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_manager {

namespace {

constexpr const char* kUsers[] = {"a@gmail.com", "b@gmail.com"};

struct BehaviorTestCase {
  MultiUserSignInPolicy primary;
  MultiUserSignInPolicy secondary;
  bool expected_secondary_allowed;
};

constexpr BehaviorTestCase kBehaviorTestCases[] = {
    {
        MultiUserSignInPolicy::kUnrestricted,
        MultiUserSignInPolicy::kUnrestricted,
        true,
    },
    {
        MultiUserSignInPolicy::kUnrestricted,
        MultiUserSignInPolicy::kPrimaryOnly,
        false,
    },
    {
        MultiUserSignInPolicy::kUnrestricted,
        MultiUserSignInPolicy::kNotAllowed,
        false,
    },
    {
        MultiUserSignInPolicy::kPrimaryOnly,
        MultiUserSignInPolicy::kUnrestricted,
        true,
    },
    {
        MultiUserSignInPolicy::kPrimaryOnly,
        MultiUserSignInPolicy::kPrimaryOnly,
        false,
    },
    {
        MultiUserSignInPolicy::kPrimaryOnly,
        MultiUserSignInPolicy::kNotAllowed,
        false,
    },
    {
        MultiUserSignInPolicy::kNotAllowed,
        MultiUserSignInPolicy::kUnrestricted,
        false,
    },
    {
        MultiUserSignInPolicy::kNotAllowed,
        MultiUserSignInPolicy::kPrimaryOnly,
        false,
    },
    {
        MultiUserSignInPolicy::kNotAllowed,
        MultiUserSignInPolicy::kNotAllowed,
        false,
    },
};

std::unique_ptr<KeyedService> TestPolicyCertServiceFactory(
    content::BrowserContext* context) {
  return policy::PolicyCertService::CreateForTesting(
      Profile::FromBrowserContext(context));
}

class MockUserManagerObserver : public UserManager::Observer {
 public:
  MOCK_METHOD(void, OnUserNotAllowed, (const std::string&), (override));
};

}  // namespace

class MultiUserSignInPolicyControllerTest : public testing::Test {
 public:
  MultiUserSignInPolicyControllerTest()
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {
    for (size_t i = 0; i < std::size(kUsers); ++i) {
      test_users_.push_back(AccountId::FromUserEmail(kUsers[i]));
    }
  }

  MultiUserSignInPolicyControllerTest(
      const MultiUserSignInPolicyControllerTest&) = delete;
  MultiUserSignInPolicyControllerTest& operator=(
      const MultiUserSignInPolicyControllerTest&) = delete;

  ~MultiUserSignInPolicyControllerTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &local_state_);
    ASSERT_TRUE(profile_manager_->SetUp());

    for (const auto& account_id : test_users_) {
      fake_user_manager_->AddUser(account_id);

      // Note that user profiles are created after user login in reality.
      TestingProfile* user_profile =
          profile_manager_->CreateTestingProfile(account_id.GetUserEmail());
      user_profile->set_profile_name(account_id.GetUserEmail());
      user_profiles_.push_back(user_profile);
    }
  }

  void TearDown() override {
    // We must ensure that the network::CertVerifierWithTrustAnchors outlives
    // the PolicyCertService so shutdown the profile here. Additionally, we need
    // to run the message loop between freeing the PolicyCertService and
    // freeing the network::CertVerifierWithTrustAnchors (see
    // PolicyCertService::OnTrustAnchorsChanged() which is called from
    // PolicyCertService::Shutdown()).
    for (const auto& account_id : test_users_) {
      fake_user_manager_->OnUserProfileWillBeDestroyed(account_id);
    }
    profile_manager_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void LoginUser(size_t user_index) {
    ASSERT_LT(user_index, test_users_.size());
    fake_user_manager_->LoginUser(test_users_[user_index], false);
    fake_user_manager_->OnUserProfileCreated(
        test_users_[user_index], user_profiles_[user_index]->GetPrefs());
  }

  void SetOwner(size_t user_index) {
    fake_user_manager_->SetOwnerId(test_users_[user_index]);
  }

  PrefService* GetUserPrefs(size_t user_index) {
    return user_profiles_[user_index]->GetPrefs();
  }

  void SetPrefBehavior(size_t user_index, MultiUserSignInPolicy policy) {
    GetUserPrefs(user_index)
        ->SetString(prefs::kMultiProfileUserBehaviorPref,
                    MultiUserSignInPolicyToPrefValue(policy));
  }

  MultiUserSignInPolicy GetCachedBehavior(size_t user_index) {
    return controller()->GetCachedValue(test_users_[user_index].GetUserEmail());
  }

  void SetCachedBehavior(size_t user_index, MultiUserSignInPolicy policy) {
    controller()->SetCachedValue(test_users_[user_index].GetUserEmail(),
                                 policy);
  }

  MultiUserSignInPolicyController* controller() {
    return fake_user_manager_->GetMultiUserSignInPolicyController();
  }

  TestingProfile* profile(int index) { return user_profiles_[index]; }

  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  content::BrowserTaskEnvironment task_environment_;
  TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  std::vector<raw_ptr<TestingProfile, VectorExperimental>> user_profiles_;

  std::vector<AccountId> test_users_;
};

// Tests that everyone is allowed before a session starts.
TEST_F(MultiUserSignInPolicyControllerTest, AllAllowedBeforeLogin) {
  constexpr MultiUserSignInPolicy kTestCases[] = {
      MultiUserSignInPolicy::kUnrestricted,
      MultiUserSignInPolicy::kPrimaryOnly,
      MultiUserSignInPolicy::kNotAllowed,
  };
  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(i);
    SetCachedBehavior(0, kTestCases[i]);
    EXPECT_TRUE(
        controller()->IsUserAllowedInSession(test_users_[0].GetUserEmail()));
    EXPECT_EQ(std::nullopt,
              GetMultiUserSignInPolicy(fake_user_manager_->GetPrimaryUser()));
  }
}

// Tests that invalid cache value would become the default "unrestricted".
TEST_F(MultiUserSignInPolicyControllerTest, InvalidCacheBecomesDefault) {
  {
    constexpr char kBad[] = "some invalid value";
    ScopedDictPrefUpdate update(
        TestingBrowserProcess::GetGlobal()->local_state(),
        prefs::kCachedMultiProfileUserBehavior);
    update->Set(test_users_[0].GetUserEmail(), kBad);
  }
  EXPECT_EQ(MultiUserSignInPolicy::kUnrestricted, GetCachedBehavior(0));
}

// Tests that cached behavior value changes with user pref after login.
TEST_F(MultiUserSignInPolicyControllerTest, CachedBehaviorUpdate) {
  LoginUser(0);

  constexpr MultiUserSignInPolicy kTestCases[] = {
      MultiUserSignInPolicy::kUnrestricted,
      MultiUserSignInPolicy::kPrimaryOnly,
      MultiUserSignInPolicy::kNotAllowed,
      MultiUserSignInPolicy::kUnrestricted,
  };
  for (const auto policy : kTestCases) {
    SetPrefBehavior(0, policy);
    EXPECT_EQ(policy, GetCachedBehavior(0));
  }
}

// Tests that compromised cache value would be fixed and pref value is checked
// upon login.
TEST_F(MultiUserSignInPolicyControllerTest, CompromisedCacheFixedOnLogin) {
  MockUserManagerObserver mock_observer;
  base::ScopedObservation<UserManager,
                          UserManager::Observer>
      observation(&mock_observer);
  observation.Observe(fake_user_manager_.Get());

  SetPrefBehavior(0, MultiUserSignInPolicy::kPrimaryOnly);
  SetCachedBehavior(0, MultiUserSignInPolicy::kUnrestricted);
  EXPECT_EQ(MultiUserSignInPolicy::kUnrestricted, GetCachedBehavior(0));

  EXPECT_CALL(mock_observer, OnUserNotAllowed(testing::_)).Times(0);
  LoginUser(0);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_EQ(MultiUserSignInPolicy::kPrimaryOnly, GetCachedBehavior(0));

  SetPrefBehavior(1, MultiUserSignInPolicy::kPrimaryOnly);
  SetCachedBehavior(1, MultiUserSignInPolicy::kUnrestricted);
  EXPECT_EQ(MultiUserSignInPolicy::kUnrestricted, GetCachedBehavior(1));
  EXPECT_CALL(mock_observer, OnUserNotAllowed(testing::_)).Times(1);
  LoginUser(1);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_EQ(MultiUserSignInPolicy::kPrimaryOnly, GetCachedBehavior(1));
}

// Tests cases before the second user login.
TEST_F(MultiUserSignInPolicyControllerTest, IsSecondaryAllowed) {
  LoginUser(0);

  for (size_t i = 0; i < std::size(kBehaviorTestCases); ++i) {
    SCOPED_TRACE(i);
    SetPrefBehavior(0, kBehaviorTestCases[i].primary);
    SetCachedBehavior(1, kBehaviorTestCases[i].secondary);

    EXPECT_EQ(kBehaviorTestCases[i].primary,
              GetMultiUserSignInPolicy(fake_user_manager_->GetPrimaryUser()));
    EXPECT_EQ(
        kBehaviorTestCases[i].expected_secondary_allowed,
        controller()->IsUserAllowedInSession(test_users_[1].GetUserEmail()));
  }
}

// Tests user behavior changes within a two-user session.
TEST_F(MultiUserSignInPolicyControllerTest, PrimaryBehaviorChange) {
  MockUserManagerObserver mock_observer;
  base::ScopedObservation<UserManager,
                          UserManager::Observer>
      observation(&mock_observer);
  observation.Observe(fake_user_manager_.Get());
  EXPECT_CALL(mock_observer, OnUserNotAllowed(testing::_))
      .Times(testing::AnyNumber());
  LoginUser(0);
  LoginUser(1);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  for (size_t i = 0; i < std::size(kBehaviorTestCases); ++i) {
    SCOPED_TRACE(i);
    EXPECT_CALL(mock_observer, OnUserNotAllowed(testing::_))
        .Times(testing::AnyNumber());
    SetPrefBehavior(0, MultiUserSignInPolicy::kUnrestricted);
    SetPrefBehavior(1, MultiUserSignInPolicy::kUnrestricted);
    testing::Mock::VerifyAndClearExpectations(&mock_observer);

    EXPECT_CALL(mock_observer, OnUserNotAllowed(testing::_))
        .Times(kBehaviorTestCases[i].expected_secondary_allowed
                   ? testing::Exactly(0)
                   : testing::AtLeast(1));
    SetPrefBehavior(0, kBehaviorTestCases[i].primary);
    SetPrefBehavior(1, kBehaviorTestCases[i].secondary);
    testing::Mock::VerifyAndClearExpectations(&mock_observer);
  }
}

TEST_F(MultiUserSignInPolicyControllerTest,
       UsedPolicyCertificatesAllowedForPrimary) {
  // Verifies that any user can sign-in as the primary user, regardless of the
  // tainted state.
  ASSERT_TRUE(
      policy::PolicyCertServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(0), base::BindRepeating(&TestPolicyCertServiceFactory)));
  policy::PolicyCertServiceFactory::GetForProfile(profile(0))
      ->SetUsedPolicyCertificates();

  EXPECT_TRUE(
      controller()->IsUserAllowedInSession(test_users_[0].GetUserEmail()));
  EXPECT_TRUE(
      controller()->IsUserAllowedInSession(test_users_[1].GetUserEmail()));
}

TEST_F(MultiUserSignInPolicyControllerTest,
       UsedPolicyCertificatesAllowedForSecondary) {
  // Verifies that if a regular user is signed-in then other regular users can
  // be added, including users that have used policy-provided trust anchors.
  LoginUser(1);

  // TODO(xiyuan): Remove the following SetPrefBehavor when default is
  // changed back to enabled.
  SetPrefBehavior(1, MultiUserSignInPolicy::kUnrestricted);

  EXPECT_TRUE(
      controller()->IsUserAllowedInSession(test_users_[0].GetUserEmail()));

  ASSERT_TRUE(
      policy::PolicyCertServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(0), base::BindRepeating(&TestPolicyCertServiceFactory)));
  policy::PolicyCertServiceFactory::GetForProfile(profile(0))
      ->SetUsedPolicyCertificates();

  EXPECT_TRUE(
      controller()->IsUserAllowedInSession(test_users_[0].GetUserEmail()));
}

TEST_F(MultiUserSignInPolicyControllerTest,
       SecondaryAllowedWhenPrimaryUsedPolicyCertificates) {
  // Verifies that if a tainted user is signed-in then other users can still be
  // added.
  ASSERT_TRUE(
      policy::PolicyCertServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(0), base::BindRepeating(&TestPolicyCertServiceFactory)));
  policy::PolicyCertServiceFactory::GetForProfile(profile(0))
      ->SetUsedPolicyCertificates();
  LoginUser(0);

  EXPECT_TRUE(
      controller()->IsUserAllowedInSession(test_users_[1].GetUserEmail()));

  ASSERT_TRUE(
      policy::PolicyCertServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(1), base::BindRepeating(&TestPolicyCertServiceFactory)));
  policy::PolicyCertServiceFactory::GetForProfile(profile(1))
      ->SetUsedPolicyCertificates();

  EXPECT_TRUE(
      controller()->IsUserAllowedInSession(test_users_[1].GetUserEmail()));

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

TEST_F(MultiUserSignInPolicyControllerTest,
       PolicyCertificatesInMemoryDisallowsSecondaries) {
  // Verifies that if a user is signed-in and has policy certificates installed
  // then other users can still be added.
  LoginUser(0);

  // TODO(xiyuan): Remove the following SetPrefBehavor when default is
  // changed back to enabled.
  SetPrefBehavior(0, MultiUserSignInPolicy::kUnrestricted);

  ASSERT_TRUE(
      policy::PolicyCertServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(0), base::BindRepeating(&TestPolicyCertServiceFactory)));
  policy::PolicyCertService* service =
      policy::PolicyCertServiceFactory::GetForProfile(profile(0));
  ASSERT_TRUE(service);

  EXPECT_FALSE(service->has_policy_certificates());
  EXPECT_TRUE(
      controller()->IsUserAllowedInSession(test_users_[1].GetUserEmail()));

  net::CertificateList certificates;
  certificates.push_back(
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem"));
  service->SetPolicyTrustAnchorsForTesting(/*trust_anchors=*/certificates);
  EXPECT_TRUE(service->has_policy_certificates());
  EXPECT_TRUE(
      controller()->IsUserAllowedInSession(test_users_[1].GetUserEmail()));

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

}  // namespace user_manager
