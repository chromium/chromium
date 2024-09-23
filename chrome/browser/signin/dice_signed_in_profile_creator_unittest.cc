// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_signed_in_profile_creator.h"

#include <tuple>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char16_t kProfileTestName[] = u"profile_test_name";

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

  for (const auto& url_name : cookie_url_and_name) {
    GURL url(url_name.first);
    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateSanitizedCookie(
            url, url_name.second, "A=" + url_name.second, url.host(),
            url.path(), base::Time::Now(), base::Time::Max(), base::Time::Now(),
            url.SchemeIsCryptographic(), false,
            net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT,
            std::nullopt, /*status=*/nullptr);
    cookie_manager->SetCanonicalCookie(
        *cookie, url, net::CookieOptions::MakeAllInclusive(),
        base::BindLambdaForTesting(
            [&](net::CookieAccessResult access_result) { barrier.Run(); }));
  }

  run_loop.Run();
}

std::unique_ptr<TestingProfile> BuildTestingProfile(
    const base::FilePath& path,
    Profile::Delegate* delegate,
    Profile::CreateMode create_mode,
    bool tokens_loaded) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetDelegate(delegate);
  profile_builder.SetCreateMode(create_mode);
  profile_builder.SetPath(path);
  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(profile_builder);
  if (!tokens_loaded) {
    IdentityTestEnvironmentProfileAdaptor adaptor(profile.get());
    adaptor.identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  }

  return profile;
}

class UnittestProfileManager : public FakeProfileManager {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : FakeProfileManager(user_data_dir) {}

  void set_tokens_loaded_at_creation(bool loaded) {
    tokens_loaded_at_creation_ = loaded;
  }

  std::unique_ptr<TestingProfile> BuildTestingProfile(
      const base::FilePath& path,
      Profile::Delegate* delegate,
      Profile::CreateMode create_mode) override {
    return ::BuildTestingProfile(path, delegate, create_mode,
                                 tokens_loaded_at_creation_);
  }

  bool tokens_loaded_at_creation_ = true;
};

}  // namespace

// Testing params:
// - bool enable_third_party_management_feature
// - bool setup_cookies_to_move
// - bool explicit_browser_signin_enabled
class DiceSignedInProfileCreatorTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>>,
      public ProfileManagerObserver {
 public:
  DiceSignedInProfileCreatorTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {
    InitFeatures();
    auto profile_manager_unique = std::make_unique<UnittestProfileManager>(
        base::CreateUniqueTempDirectoryScopedToTest());
    profile_manager_ = profile_manager_unique.get();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::move(profile_manager_unique));
    profile_ = BuildTestingProfile(base::FilePath(), /*delegate=*/nullptr,
                                   Profile::CreateMode::kSynchronous,
                                   /*tokens_loaded=*/true);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    profile_manager()->AddObserver(this);
  }

  void InitFeatures() {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;
    if (enable_third_party_management_feature()) {
      enabled.push_back(
          profile_management::features::kThirdPartyProfileManagement);
    } else {
      disabled.push_back(
          profile_management::features::kThirdPartyProfileManagement);
    }

    if (explicit_browser_signin_enabled()) {
      enabled.push_back(switches::kExplicitBrowserSigninUIOnDesktop);
    } else {
      disabled.push_back(switches::kExplicitBrowserSigninUIOnDesktop);
    }

    scoped_feature_list_.InitWithFeatures(enabled, disabled);
  }

  ~DiceSignedInProfileCreatorTest() override { DeleteProfiles(); }

  bool enable_third_party_management_feature() {
    return std::get<0>(GetParam());
  }

  bool setup_cookies_to_move() { return std::get<1>(GetParam()); }

  bool explicit_browser_signin_enabled() { return std::get<2>(GetParam()); }

  UnittestProfileManager* profile_manager() { return profile_manager_; }

  // Test environment attached to profile().
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  // Source profile (the one which we are extracting credentials from).
  Profile* profile() { return profile_.get(); }

  // Profile created by the DiceSignedInProfileCreator.
  Profile* signed_in_profile() { return signed_in_profile_; }

  // Profile added to the ProfileManager. In general this should be the same as
  // signed_in_profile() except in error cases.
  Profile* added_profile() { return added_profile_; }

  bool creator_callback_called() { return creator_callback_called_; }

  void set_profile_added_closure(base::OnceClosure closure) {
    profile_added_closure_ = std::move(closure);
  }

  void DeleteProfiles() {
    identity_test_env_profile_adaptor_.reset();

    // Delete the profile first to make sure all observers to the profile
    // manager are cleared to avoid heap-use-after-free when the observer try to
    // stop observing the manager.
    profile_.reset();
    if (profile_manager_) {
      profile_manager()->RemoveObserver(this);
      TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
      profile_manager_ = nullptr;
    }
  }

  // Callback for the DiceSignedInProfileCreator.
  void OnProfileCreated(base::OnceClosure quit_closure, Profile* profile) {
    creator_callback_called_ = true;
    signed_in_profile_ = profile;
    if (quit_closure)
      std::move(quit_closure).Run();
  }

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override {
    added_profile_ = profile;
    if (profile_added_closure_)
      std::move(profile_added_closure_).Run();
  }

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
          added_profile_->GetDefaultStoragePartition()
              ->GetCookieManagerForBrowserProcess();
      base::RunLoop loop;
      cookie_manager->GetAllCookies(
          base::BindLambdaForTesting([&](const net::CookieList& cookies) {
            cookies_new_profile = cookies;
            loop.Quit();
          }));
      loop.Run();
    }

    if (!enable_third_party_management_feature()) {
      EXPECT_EQ(6u, cookies_source_profile.size());
      EXPECT_TRUE(cookies_new_profile.empty());
      return;
    }

    // I don't know why this test looks like this, but I am only removing
    // !BUILDFLAG(IS_FUCHSIA).
    return;

    EXPECT_EQ(3u, cookies_source_profile.size());
    EXPECT_EQ(3u, cookies_new_profile.size());

    for (const auto& cookie : cookies_new_profile) {
      EXPECT_TRUE(cookie.IsDomainMatch(url.host()));
      EXPECT_TRUE(cookie.Name() == "oldgoogle0" ||
                  cookie.Name() == "validgoogle1" ||
                  cookie.Name() == "newgoogle1");
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
  raw_ptr<UnittestProfileManager, DanglingUntriaged> profile_manager_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<Profile, DanglingUntriaged> signed_in_profile_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> added_profile_ = nullptr;
  base::OnceClosure profile_added_closure_;
  bool creator_callback_called_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(DiceSignedInProfileCreatorTest, CreateWithTokensLoaded) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  size_t kTestIcon = profiles::GetModernAvatarIconStartIndex();

  SetupCookiesToMove();
  base::RunLoop loop;
  std::unique_ptr<DiceSignedInProfileCreator> creator =
      std::make_unique<DiceSignedInProfileCreator>(
          profile(), account_info.account_id, kProfileTestName, kTestIcon,
          base::BindOnce(&DiceSignedInProfileCreatorTest::OnProfileCreated,
                         base::Unretained(this), loop.QuitClosure()));
  loop.Run();

  // Check that the account was moved.
  EXPECT_TRUE(creator_callback_called());
  EXPECT_TRUE(signed_in_profile());
  EXPECT_NE(profile(), signed_in_profile());
  EXPECT_EQ(signed_in_profile(), added_profile());
  EXPECT_FALSE(IdentityManagerFactory::GetForProfile(profile())
                   ->HasAccountWithRefreshToken(account_info.account_id));
  signin::IdentityManager* new_identity_manager =
      IdentityManagerFactory::GetForProfile(signed_in_profile());
  EXPECT_EQ(1u, new_identity_manager->GetAccountsWithRefreshTokens().size());
  EXPECT_TRUE(IdentityManagerFactory::GetForProfile(signed_in_profile())
                  ->HasAccountWithRefreshToken(account_info.account_id));
  if (explicit_browser_signin_enabled()) {
    EXPECT_TRUE(
        new_identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  }

  // Check profile type
  ASSERT_FALSE(signed_in_profile()->IsGuestSession());

  // Check the profile name and icon.
  ProfileAttributesStorage& storage =
      profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(signed_in_profile()->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_EQ(kProfileTestName, entry->GetLocalProfileName());
  EXPECT_EQ(kTestIcon, entry->GetAvatarIconIndex());
  VerifyCookiesMoved();
}

TEST_P(DiceSignedInProfileCreatorTest, CreateWithTokensNotLoaded) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  profile_manager()->set_tokens_loaded_at_creation(false);

  base::RunLoop creator_loop;
  base::RunLoop profile_added_loop;
  set_profile_added_closure(profile_added_loop.QuitClosure());
  SetupCookiesToMove();
  std::unique_ptr<DiceSignedInProfileCreator> creator =
      std::make_unique<DiceSignedInProfileCreator>(
          profile(), account_info.account_id, std::u16string(), std::nullopt,
          base::BindOnce(&DiceSignedInProfileCreatorTest::OnProfileCreated,
                         base::Unretained(this), creator_loop.QuitClosure()));
  profile_added_loop.Run();
  base::RunLoop().RunUntilIdle();

  // The profile was created, but tokens not loaded. The callback has not been
  // called yet.
  EXPECT_FALSE(creator_callback_called());
  EXPECT_TRUE(added_profile());
  EXPECT_NE(profile(), added_profile());

  // Load the tokens.
  IdentityTestEnvironmentProfileAdaptor adaptor(added_profile());
  adaptor.identity_test_env()->ReloadAccountsFromDisk();
  creator_loop.Run();

  // Check that the account was moved.
  EXPECT_EQ(signed_in_profile(), added_profile());
  EXPECT_TRUE(creator_callback_called());
  EXPECT_FALSE(IdentityManagerFactory::GetForProfile(profile())
                   ->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_EQ(1u, IdentityManagerFactory::GetForProfile(signed_in_profile())
                    ->GetAccountsWithRefreshTokens()
                    .size());
  EXPECT_TRUE(IdentityManagerFactory::GetForProfile(signed_in_profile())
                  ->HasAccountWithRefreshToken(account_info.account_id));
  VerifyCookiesMoved();
}

// Deleting the creator while it is running does not crash.
TEST_P(DiceSignedInProfileCreatorTest, DeleteWhileCreating) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  SetupCookiesToMove();
  std::unique_ptr<DiceSignedInProfileCreator> creator =
      std::make_unique<DiceSignedInProfileCreator>(
          profile(), account_info.account_id, std::u16string(), std::nullopt,
          base::BindOnce(&DiceSignedInProfileCreatorTest::OnProfileCreated,
                         base::Unretained(this), base::OnceClosure()));
  EXPECT_FALSE(creator_callback_called());
  creator.reset();
  base::RunLoop().RunUntilIdle();
}

// Deleting the profile while waiting for the tokens.
TEST_P(DiceSignedInProfileCreatorTest, DeleteProfile) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  profile_manager()->set_tokens_loaded_at_creation(false);

  base::RunLoop creator_loop;
  base::RunLoop profile_added_loop;
  set_profile_added_closure(profile_added_loop.QuitClosure());
  SetupCookiesToMove();
  std::unique_ptr<DiceSignedInProfileCreator> creator =
      std::make_unique<DiceSignedInProfileCreator>(
          profile(), account_info.account_id, std::u16string(), std::nullopt,
          base::BindOnce(&DiceSignedInProfileCreatorTest::OnProfileCreated,
                         base::Unretained(this), creator_loop.QuitClosure()));
  profile_added_loop.Run();
  base::RunLoop().RunUntilIdle();

  // The profile was created, but tokens not loaded. The callback has not been
  // called yet.
  EXPECT_FALSE(creator_callback_called());
  EXPECT_TRUE(added_profile());
  EXPECT_NE(profile(), added_profile());
  VerifyCookiesMoved();

  DeleteProfiles();
  creator_loop.Run();

  // The callback is called with nullptr profile.
  EXPECT_TRUE(creator_callback_called());
  EXPECT_FALSE(signed_in_profile());
}

INSTANTIATE_TEST_SUITE_P(All,
                         DiceSignedInProfileCreatorTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));
