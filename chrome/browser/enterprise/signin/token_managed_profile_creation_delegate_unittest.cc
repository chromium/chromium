// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/token_managed_profile_creation_delegate.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kEnrollmentToken[] = "fake-enrollment-token";

void CreateCookies(
    Profile* profile,
    const std::map<std::string, std::string> cookie_url_and_name) {
  network::mojom::CookieManager* cookie_manager =
      profile->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();

  base::RunLoop run_loop;
  base::RepeatingClosure barrier = base::BarrierClosure(
      cookie_url_and_name.size(),
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  for (const auto& [url_string, name] : cookie_url_and_name) {
    GURL url(url_string);
    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateSanitizedCookie(
            url, name, "A=" + name, url.host(), url.path(), base::Time::Now(),
            base::Time::Max(), base::Time::Now(), url.SchemeIsCryptographic(),
            false, net::CookieSameSite::NO_RESTRICTION,
            net::COOKIE_PRIORITY_DEFAULT, std::nullopt, /*status=*/nullptr);
    cookie_manager->SetCanonicalCookie(
        *cookie, url, net::CookieOptions::MakeAllInclusive(),
        base::BindLambdaForTesting(
            [&](net::CookieAccessResult access_result) { barrier.Run(); }));
  }

  run_loop.Run();
}

}  // namespace

class TokenManagedProfileCreationDelegateTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  TokenManagedProfileCreationDelegateTest()
      : profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())) {
    feature_list_.InitWithFeatureState(
        profile_management::features::kThirdPartyProfileManagement,
        enable_third_party_management_feature());
  }

  ~TokenManagedProfileCreationDelegateTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test_profile");
  }

  // Callback for the TokenManagedProfileCreationDelegate and profile creator.
  void OnProfileCreated(base::OnceClosure quit_closure,
                        base::WeakPtr<Profile> profile) {
    created_profile_ = profile.get();
    if (quit_closure) {
      std::move(quit_closure).Run();
    }
  }

  bool enable_third_party_management_feature() {
    return std::get<0>(GetParam());
  }

  bool setup_cookies_to_move() { return std::get<1>(GetParam()); }

  void SetupCookiesToMove() {
    if (!setup_cookies_to_move()) {
      return;
    }
    // Add some cookies
    CreateCookies(profile_.get(), {{"https://google.com", "oldgoogle0"},
                                   {"https://example.com", "oldexample0"}});
    CreateCookies(profile_.get(), {{"https://google.com", "validgoogle1"},
                                   {"https://example.com", "validexample1"}});
    CreateCookies(profile_.get(), {{"https://google.com", "newgoogle2"},
                                   {"https://example.com", "newexample2"}});

    profile_->GetPrefs()->SetString(prefs::kSigninInterceptionIDPCookiesUrl,
                                    "https://www.google.com/");
  }

  void VerifyCookiesMoved() {
    if (!setup_cookies_to_move()) {
      return;
    }
    GURL url("https://www.google.com/");
    net::CookieList cookies_source_profile;
    net::CookieList cookies_new_profile;
    {
      network::mojom::CookieManager* cookie_manager =
          profile_->GetDefaultStoragePartition()
              ->GetCookieManagerForBrowserProcess();
      base::RunLoop loop;
      cookie_manager->GetAllCookies(
          base::BindLambdaForTesting([&](const net::CookieList& cookies) {
            cookies_source_profile = cookies;
            loop.Quit();
          }));
      loop.Run();
    }
    {
      network::mojom::CookieManager* cookie_manager =
          created_profile_->GetDefaultStoragePartition()
              ->GetCookieManagerForBrowserProcess();
      base::RunLoop loop;
      cookie_manager->GetAllCookies(
          base::BindLambdaForTesting([&](const net::CookieList& cookies) {
            cookies_new_profile = cookies;
            loop.Quit();
          }));
      loop.Run();
    }

    if (!base::FeatureList::IsEnabled(
            profile_management::features::kThirdPartyProfileManagement)) {
      EXPECT_EQ(6u, cookies_source_profile.size());
      EXPECT_TRUE(cookies_new_profile.empty());
      return;
    }

    EXPECT_EQ(3u, cookies_source_profile.size());
    EXPECT_EQ(3u, cookies_new_profile.size());

    for (const auto& cookie : cookies_new_profile) {
      EXPECT_TRUE(cookie.IsDomainMatch(url.host()));
      EXPECT_TRUE(cookie.Name() == "oldgoogle0" ||
                  cookie.Name() == "validgoogle1" ||
                  cookie.Name() == "newgoogle2");
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile> profile_;
  raw_ptr<Profile> created_profile_;
  bool creator_callback_called_ = false;
};

TEST_P(TokenManagedProfileCreationDelegateTest,
  CreatesProfileWithManagementInfo) {
  auto delegate = std::make_unique<TokenManagedProfileCreationDelegate>(
      kEnrollmentToken);

  auto* entry = TestingBrowserProcess::GetGlobal()
                    ->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile_->GetPath());
  delegate->SetManagedAttributesForProfile(entry);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kEnrollmentToken, entry->GetProfileManagementEnrollmentToken());
}

TEST_P(TokenManagedProfileCreationDelegateTest, MovingCookies) {
  SetupCookiesToMove();
  base::RunLoop loop;
  auto delegate = std::make_unique<TokenManagedProfileCreationDelegate>(
    kEnrollmentToken
  );
  created_profile_ = profile_manager_->CreateTestingProfile("new_test_profile");
  delegate->OnManagedProfileInitialized(
    profile_,
    created_profile_,
    base::BindOnce(&TokenManagedProfileCreationDelegateTest::OnProfileCreated,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();

  VerifyCookiesMoved();
}

INSTANTIATE_TEST_SUITE_P(All,
                         TokenManagedProfileCreationDelegateTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));
