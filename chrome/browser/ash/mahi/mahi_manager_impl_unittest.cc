// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_impl.h"

#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"
#include "chrome/browser/ash/mahi/mahi_cache_manager.h"
#include "chrome/browser/chromeos/mahi/test/fake_mahi_web_contents_manager.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/lottie/resource.h"
#include "ui/views/widget/widget.h"

namespace {

using mahi::FakeMahiWebContentsManager;
using ::testing::IsNull;

constexpr char kFakeSummary[] = "Fake summary";
constexpr char kFakeContent[] = "Test page content";

class FakeMahiProvider : public manta::MahiProvider {
 public:
  FakeMahiProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : MahiProvider(std::move(test_url_loader_factory), identity_manager) {}

  void Summarize(const std::string& input,
                 const std::string& title,
                 const std::optional<std::string>& url,
                 manta::MantaGenericCallback callback) override {
    ++num_summarize_call_;
    latest_title_ = title;
    latest_url_ = url;
    std::move(callback).Run(base::Value::Dict().Set("outputData", kFakeSummary),
                            {manta::MantaStatusCode::kOk, "Status string ok"});
  }

  // Counts the number of call to `Summarize()`
  int NumberOfSumarizeCall() { return num_summarize_call_; }

  const std::string& latest_title() const { return latest_title_; }
  const std::optional<std::string>& latest_url() const { return latest_url_; }

 private:
  int num_summarize_call_ = 0;
  std::string latest_title_;
  std::optional<std::string> latest_url_;
};

bool IsMahiNudgeShown() {
  return ash::Shell::Get()->anchored_nudge_manager()->IsNudgeShown(
      ash::mahi_constants::kMahiNudgeId);
}

}  // namespace

namespace ash {

class MahiManagerImplTest : public NoSessionAshTestBase {
 public:
  MahiManagerImplTest()
      : NoSessionAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Sets the default functions for the test to create image with the lottie
    // resource id. Otherwise there's no `g_parse_lottie_as_still_image_` set in
    // the `ResourceBundle`.
    ui::ResourceBundle::SetLottieParsingFunctions(
        &lottie::ParseLottieAsStillImage,
        &lottie::ParseLottieAsThemedStillImage);
  }

  MahiManagerImplTest(const MahiManagerImplTest&) = delete;
  MahiManagerImplTest& operator=(const MahiManagerImplTest&) = delete;

  ~MahiManagerImplTest() override = default;

  // NoSessionAshTestBase::
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kMahi,
                              chromeos::features::kFeatureManagementMahi},
        /*disabled_features=*/{});
    NoSessionAshTestBase::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kMahiRestrictionsOverride);

    magic_boost_state_ = std::make_unique<MagicBoostStateAsh>();
    mahi_manager_impl_ = std::make_unique<MahiManagerImpl>();
    mahi_manager_impl_->mahi_provider_ = CreateMahiProvider();

    CreateUserSessions(1);
  }

  void TearDown() override {
    mahi_manager_impl_.reset();
    magic_boost_state_.reset();
    NoSessionAshTestBase::TearDown();
  }

  void SetMahiEnabledByUserPref(bool enabled) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        ash::prefs::kHmrEnabled, enabled);
  }

  FakeMahiProvider* GetMahiProvider() {
    return static_cast<FakeMahiProvider*>(
        mahi_manager_impl_->mahi_provider_.get());
  }

  mahi::FakeMahiWebContentsManager* GetFakeMahiWebContentsManager() {
    return static_cast<FakeMahiWebContentsManager*>(
        chromeos::MahiWebContentsManager::Get());
  }

  bool IsEnabled() const { return mahi_manager_impl_->IsEnabled(); }

  crosapi::mojom::MahiPageInfoPtr CreatePageInfo(const std::string& url,
                                                 const std::u16string& title,
                                                 bool is_incognito = false) {
    return crosapi::mojom::MahiPageInfo::New(
        /*client_id=*/base::UnguessableToken(),
        /*page_id=*/base::UnguessableToken(), /*url=*/GURL(url),
        /*title=*/title,
        /*favicon_image=*/gfx::ImageSkia(), /*is_distillable=*/true,
        /*is_incognito=*/is_incognito);
  }

  MahiCacheManager* GetCacheManager() {
    return mahi_manager_impl_->cache_manager_.get();
  }

  void NotifyRefreshAvailability(bool available) {
    mahi_manager_impl_->NotifyRefreshAvailability(available);
  }

  void RequestContent(bool incognito = false,
                      const std::string& url = "http://url1.com/abc#skip") {
    // Sets the page that needed to get summary.
    mahi_manager_impl_->SetCurrentFocusedPageInfo(
        CreatePageInfo(url, /*title=*/u"Title of url1",
                       /*is_incognito=*/incognito));
    // Gets the content of the page.
    mahi_manager_impl_->GetContent(base::DoNothing());
  }

  // void RequestSummary(const std::string& url = "http://url1.com/abc#skip",
  // bool incognito = false) {
  void RequestSummary(bool incognito = false,
                      const std::string& url = "http://url1.com/abc#skip") {
    // Sets the page that needed to get summary.
    mahi_manager_impl_->SetCurrentFocusedPageInfo(
        CreatePageInfo(url, /*title=*/u"Title of url1",
                       /*is_incognito=*/incognito));
    // Gets the summary of the page.
    mahi_manager_impl_->GetSummary(base::DoNothing());
  }

 protected:
  std::unique_ptr<FakeMahiProvider> CreateMahiProvider() {
    return std::make_unique<FakeMahiProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_.identity_manager());
  }
  std::unique_ptr<MagicBoostStateAsh> magic_boost_state_;
  std::unique_ptr<MahiManagerImpl> mahi_manager_impl_;
  base::test::ScopedFeatureList feature_list_;

 private:
  mahi::FakeMahiWebContentsManager fake_mahi_web_contents_manager_;
  chromeos::ScopedMahiWebContentsManagerOverride
      scoped_mahi_web_contents_manager_{&fake_mahi_web_contents_manager_};
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(MahiManagerImplTest, CacheSavedForContentRequest) {
  // No cache yet.
  EXPECT_EQ(GetCacheManager()->size(), 0);

  RequestContent();

  // Summary is saved in the cache.
  EXPECT_EQ(GetCacheManager()->size(), 1);
  auto content = GetCacheManager()->GetPageContentForUrl("http://url1.com/abc");
  EXPECT_EQ(base::UTF16ToUTF8(content), kFakeContent);
}

TEST_F(MahiManagerImplTest, NoContentCacheSavedForIncognitoPage) {
  // No cache at the beginning.
  EXPECT_EQ(GetCacheManager()->size(), 0);

  // Request content from a incognito page.
  RequestContent(/*incognito=*/true);

  // Content is not saved in the cache.
  EXPECT_EQ(GetCacheManager()->size(), 0);

  // Request content from a normal page.
  RequestSummary(/*incognito=*/false);
  // Content is saved in the cache.
  EXPECT_EQ(GetCacheManager()->size(), 1);
}

TEST_F(MahiManagerImplTest, NoContentCallWhenContentIsInCache) {
  // Adds some content to the cache.
  const std::u16string new_summary(u"new summary");
  const std::u16string new_content(u"Page content");
  GetCacheManager()->AddCacheForUrl(
      "http://url1.com/abc#random",
      MahiCacheManager::MahiData(
          /*url=*/"http://url1.com/abc#skip", /*title=*/u"Title of url1",
          /*page_content=*/new_content, /*favicon_image=*/std::nullopt,
          /*summary=*/new_summary, /*previous_qa=*/{}));

  RequestContent();

  auto content = GetCacheManager()->GetPageContentForUrl("http://url1.com/abc");
  EXPECT_EQ(GetFakeMahiWebContentsManager()->GetNumberOfRequestContentCalls(),
            0);
  EXPECT_EQ(content, new_content);
}

// Title is included in the request proto.
TEST_F(MahiManagerImplTest, SendingTitleOnly) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kMahi,
                            chromeos::features::kFeatureManagementMahi},
      /*disabled_features=*/{chromeos::features::kMahiSendingUrl});
  RequestSummary();

  EXPECT_EQ(GetMahiProvider()->latest_title(), "Title of url1");
  EXPECT_FALSE(GetMahiProvider()->latest_url().has_value());
}

// Url, on the other hand, is controlled by kMahiSendingUrl.
TEST_F(MahiManagerImplTest, SendingTitleAndUrl) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {chromeos::features::kMahi, chromeos::features::kMahiSendingUrl,
       chromeos::features::kFeatureManagementMahi},
      /*disabled_features=*/{});

  RequestSummary();

  EXPECT_TRUE(GetMahiProvider()->latest_url().has_value());
  EXPECT_EQ(GetMahiProvider()->latest_url().value(),
            "http://url1.com/abc#skip");

  // The fake url we make up for media app pdf files is ignored.
  RequestSummary(false, "file:///media-app/example.pdf");
  EXPECT_FALSE(GetMahiProvider()->latest_url().has_value());
}

TEST_F(MahiManagerImplTest, CacheSavedForSummaryRequest) {
  // No cache yet.
  EXPECT_EQ(GetCacheManager()->size(), 0);

  RequestSummary();

  // Summary is saved in the cache.
  EXPECT_EQ(GetCacheManager()->size(), 1);
  auto summary = GetCacheManager()->GetSummaryForUrl("http://url1.com/abc");
  EXPECT_EQ(GetMahiProvider()->NumberOfSumarizeCall(), 1);
  EXPECT_TRUE(summary.has_value());
  EXPECT_EQ(base::UTF16ToUTF8(summary.value()), kFakeSummary);
}

TEST_F(MahiManagerImplTest, NoSummaryCacheSavedForIncognitoPage) {
  // No cache at the beginning.
  EXPECT_EQ(GetCacheManager()->size(), 0);

  // Request summary from a incognito page.
  RequestSummary(/*incognito=*/true);

  // Summary is not saved in the cache.
  EXPECT_EQ(GetCacheManager()->size(), 0);

  // Request summary from a normal page.
  RequestSummary(/*incognito=*/false);
  // Summary is saved in the cache.
  EXPECT_EQ(GetCacheManager()->size(), 1);
}

TEST_F(MahiManagerImplTest, NoSummaryCallWhenSummaryIsInCache) {
  // Adds some content to the cache.
  const std::u16string new_summary(u"new summary");
  GetCacheManager()->AddCacheForUrl(
      "http://url1.com/abc#random",
      MahiCacheManager::MahiData(
          /*url=*/"http://url1.com/abc#skip", /*title=*/u"Title of url1",
          /*page_content=*/u"Page content", /*favicon_image=*/std::nullopt,
          /*summary=*/new_summary, /*previous_qa=*/{}));

  RequestSummary();

  auto summary = GetCacheManager()->GetSummaryForUrl("http://url1.com/abc");
  // No call is made to MahiProvider.
  EXPECT_EQ(GetMahiProvider()->NumberOfSumarizeCall(), 0);
  EXPECT_TRUE(summary.has_value());
  EXPECT_EQ(summary.value(), new_summary);
}

TEST_F(MahiManagerImplTest, ClearAllCacheWhenAllHistoryAreBeingCleared) {
  // No cache yet.
  EXPECT_EQ(GetCacheManager()->size(), 0);

  RequestSummary();

  // Summary is saved in the cache.
  EXPECT_EQ(GetCacheManager()->size(), 1);

  mahi_manager_impl_->OnHistoryDeletions(
      nullptr, history::DeletionInfo::ForAllHistory());

  // Cache should be empty
  EXPECT_EQ(GetCacheManager()->size(), 0);
}

TEST_F(MahiManagerImplTest, ClearURLs) {
  // No cache yet.
  EXPECT_EQ(GetCacheManager()->size(), 0);

  RequestSummary();

  // Summary is saved in the cache.
  EXPECT_EQ(GetCacheManager()->size(), 1);

  // Try to delete URLs that aren't in the cache.
  {
    const auto kUrl1 = GURL("http://www.a.com");
    const auto kUrl2 = GURL("http://www.b.com");
    history::URLRows urls_to_delete = {history::URLRow(kUrl1),
                                       history::URLRow(kUrl2)};
    history::DeletionInfo deletion_info =
        history::DeletionInfo::ForUrls(urls_to_delete, std::set<GURL>());
    mahi_manager_impl_->OnHistoryDeletions(nullptr, deletion_info);

    // Cache size doesn't change.
    EXPECT_EQ(GetCacheManager()->size(), 1);
  }

  // List of URLs contains a URL that is in the cache.
  {
    const auto kUrl1 = GURL("http://www.a.com");
    const auto kUrl2 = GURL("http://url1.com/abc#should_delete");
    history::URLRows urls_to_delete = {history::URLRow(kUrl1),
                                       history::URLRow(kUrl2)};
    history::DeletionInfo deletion_info =
        history::DeletionInfo::ForUrls(urls_to_delete, std::set<GURL>());
    mahi_manager_impl_->OnHistoryDeletions(nullptr, deletion_info);

    // The URL should be deleted from the cache.
    EXPECT_EQ(GetCacheManager()->size(), 0);
  }
}

TEST_F(MahiManagerImplTest, TurnOffSettingsClearCache) {
  // No cache yet.
  EXPECT_EQ(GetCacheManager()->size(), 0);

  RequestSummary();

  // Summary is saved in the cache.
  EXPECT_EQ(GetCacheManager()->size(), 1);

  // Cache must be empty after user turn off the settings.
  SetMahiEnabledByUserPref(false);
  EXPECT_EQ(GetCacheManager()->size(), 0);
}

TEST_F(MahiManagerImplTest, ClearCacheSuccessfully) {
  // No cache yet.
  EXPECT_EQ(GetCacheManager()->size(), 0);

  RequestSummary();

  // Summary is saved in the cache.
  EXPECT_EQ(GetCacheManager()->size(), 1);

  // Cache must be empty after cleared.
  mahi_manager_impl_->ClearCache();
  EXPECT_EQ(GetCacheManager()->size(), 0);
}

TEST_F(MahiManagerImplTest, SetMahiPrefOnLogin) {
  // Checks that it should work for both when the first user's default pref is
  // true or false.
  for (bool mahi_enabled : {false, true}) {
    // Sets the pref for the default user.
    SetMahiEnabledByUserPref(mahi_enabled);
    ASSERT_EQ(IsEnabled(), mahi_enabled);
    const AccountId user1_account_id =
        Shell::Get()->session_controller()->GetActiveAccountId();

    // Sets the pref for the second user.
    SimulateUserLogin("other@user.test");
    SetMahiEnabledByUserPref(!mahi_enabled);
    EXPECT_EQ(IsEnabled(), !mahi_enabled);

    // Switching back to the previous user will update to correct pref.
    GetSessionControllerClient()->SwitchActiveUser(user1_account_id);
    EXPECT_EQ(IsEnabled(), mahi_enabled);

    // Clears all logins and re-logins the default user.
    GetSessionControllerClient()->Reset();
    SimulateUserLogin(user1_account_id);
  }
}

TEST_F(MahiManagerImplTest, OnPreferenceChanged) {
  for (bool mahi_enabled : {false, true, false}) {
    SetMahiEnabledByUserPref(mahi_enabled);
    EXPECT_EQ(IsEnabled(), mahi_enabled);
  }
}

// Tests that the Mahi educational nudge is shown when the user visits eligible
// content and they have not opted in to the feature.
TEST_F(MahiManagerImplTest, ShowEducationalNudge) {
  SetMahiEnabledByUserPref(false);

  EXPECT_FALSE(IsMahiNudgeShown());

  // Notifying that a refresh is not available should have no effect.
  NotifyRefreshAvailability(/*available=*/false);
  EXPECT_FALSE(IsMahiNudgeShown());

  // Notifying that a refresh is available should show the nudge.
  NotifyRefreshAvailability(/*available=*/true);
  EXPECT_TRUE(IsMahiNudgeShown());

  // Notifying that a refresh is not available should have no effect.
  NotifyRefreshAvailability(/*available=*/false);
  EXPECT_TRUE(IsMahiNudgeShown());
}

}  // namespace ash
