// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/essential_search/essential_search_manager.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cookies/canonical_cookie.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::test {
namespace {
constexpr char kEmail[] = "test-user@example.com";
constexpr char16_t kEmail16[] = u"test-user@example.com";
constexpr char kValidCookieHeader[] =
    "%s=%s; Domain=.google.com; Expires=Thu, 01-Jan-2026 00:00:00 GMT; Path=/; "
    "Secure; SameSite=lax";
constexpr char kCookieName[] = "SOCS";
constexpr char kCookieValue[] = "SOCS_VALUE";
}  // namespace

class EssentialSearchManagerTest : public testing::Test {
 protected:
  EssentialSearchManagerTest();
  ~EssentialSearchManagerTest() override;

  // testing::Test:
  void SetUp() override;

  void CreateEssentialSearchManager();
  void DestroyEssentialSearchManager();

  net::CookieList GetCookiesInUserProfile();

  void ExpectSocsCookieInUserProfile(const std::string& cookie_name,
                                     const std::string& cookie_value);

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;

  std::unique_ptr<app_list::EssentialSearchManager> essential_search_manager_;
};

EssentialSearchManagerTest::EssentialSearchManagerTest()
    : essential_search_manager_(nullptr) {}

EssentialSearchManagerTest::~EssentialSearchManagerTest() = default;

void EssentialSearchManagerTest::SetUp() {
  profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager_->SetUp());
  profile_ = profile_manager_->CreateTestingProfile(
      kEmail, /*prefs=*/{}, kEmail16,
      /*avatar_id=*/0,
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactories());
  identity_test_env_adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
  identity_test_env_ = identity_test_env_adaptor_->identity_test_env();
}

void EssentialSearchManagerTest::CreateEssentialSearchManager() {
  essential_search_manager_ =
      std::make_unique<app_list::EssentialSearchManager>(profile_);
}

net::CookieList EssentialSearchManagerTest::GetCookiesInUserProfile() {
  base::test::TestFuture<const net::CookieList&> future;
  profile_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->GetAllCookies(future.GetCallback());

  return future.Take();
}

void EssentialSearchManagerTest::ExpectSocsCookieInUserProfile(
    const std::string& cookie_name,
    const std::string& cookie_value) {
  net::CookieList cookie_list = GetCookiesInUserProfile();
  EXPECT_GT(cookie_list.size(), 0u);

  const auto socs_cookie_iterator = base::ranges::find(
      cookie_list, cookie_name,
      [](const net::CanonicalCookie& cookie) { return cookie.Name(); });
  ASSERT_NE(socs_cookie_iterator, cookie_list.end());
  EXPECT_EQ(cookie_value, socs_cookie_iterator->Value());
}

TEST_F(EssentialSearchManagerTest, OnCookieFetchedSucceed) {
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  // Enable EssentialSearchEnabled policy
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kEssentialSearchEnabled, base::Value(true));

  CreateEssentialSearchManager();

  base::RunLoop run_loop;
  essential_search_manager_->set_cookie_insertion_closure_for_test(
      run_loop.QuitClosure());

  // Add SOCS cookie to the user profile
  std::string cookie_header =
      base::StringPrintf(kValidCookieHeader, kCookieName, kCookieValue);
  essential_search_manager_->OnCookieFetched(cookie_header);
  run_loop.Run();

  ExpectSocsCookieInUserProfile(kCookieName, kCookieValue);
}

TEST_F(EssentialSearchManagerTest, PolicyDisabledWhileUserInSession) {
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  // Enable EssentialSearchEnabled policy
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kEssentialSearchEnabled, base::Value(true));

  CreateEssentialSearchManager();

  // Set run loops.
  base::RunLoop insertion_loop, deletion_loop;
  essential_search_manager_->set_cookie_insertion_closure_for_test(
      insertion_loop.QuitClosure());
  essential_search_manager_->set_cookie_deletion_closure_for_test(
      deletion_loop.QuitClosure());

  // Add SOCS cookie to the user profile
  std::string cookie_header =
      base::StringPrintf(kValidCookieHeader, kCookieName, kCookieValue);
  essential_search_manager_->OnCookieFetched(cookie_header);
  insertion_loop.Run();

  ExpectSocsCookieInUserProfile(kCookieName, kCookieValue);

  // Disable EssentialSearchEnabled policy
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kEssentialSearchEnabled, base::Value(false));

  // Wait for cookie deletion.
  deletion_loop.Run();

  // Expect SOCS cookie being removed from user profile.
  net::CookieList cookie_list = GetCookiesInUserProfile();
  EXPECT_THAT(cookie_list, testing::IsEmpty());
}

TEST_F(EssentialSearchManagerTest, PolicyDisabledWhileFetchingCookie) {
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  // Simulate policy being disabled while fetching the cookie.
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kEssentialSearchEnabled, base::Value(false));

  CreateEssentialSearchManager();

  // Set run loops.
  base::RunLoop insertion_loop, deletion_loop;
  essential_search_manager_->set_cookie_insertion_closure_for_test(
      insertion_loop.QuitClosure());
  essential_search_manager_->set_cookie_deletion_closure_for_test(
      deletion_loop.QuitClosure());

  // Add SOCS cookie to the user profile.
  std::string cookie_header =
      base::StringPrintf(kValidCookieHeader, kCookieName, kCookieValue);
  essential_search_manager_->OnCookieFetched(cookie_header);
  insertion_loop.Run();

  // Wait for cookie deletion.
  deletion_loop.Run();

  // Expect SOCS cookie being removed from user profile.
  net::CookieList cookie_list = GetCookiesInUserProfile();
  EXPECT_EQ(cookie_list.size(), 0u);
}

}  // namespace ash::test
