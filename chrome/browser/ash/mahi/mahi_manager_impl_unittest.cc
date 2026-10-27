// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/test/mock_mahi_media_app_content_manager.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state.h"
#include "chrome/browser/ash/mahi/mahi_cache_manager.h"
#include "chrome/browser/ash/mahi/web_contents/test_support/fake_mahi_web_contents_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/manta/manta_service_provider.h"
#include "chromeos/ash/components/signin/fake_identity_manager_provider.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_types.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/manta/manta_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/startup_visibility.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/test_variations_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"

namespace {

using base::test::RunOnceCallback;
using mahi::FakeMahiWebContentsManager;
using ::testing::_;
using ::testing::IsNull;
using ::testing::Return;

constexpr char kFakeSummary[] = "Fake summary";
constexpr char kFakeContent[] = "Test page content";
constexpr char kFakeElucidation[] = "Fake elucidation";
constexpr char kTestEmail[] = "user@example.com";
constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId(
        kTestEmail,
        GaiaId::Literal("gaia_id_for_user"));

class TestMantaServiceProvider : public ash::MantaServiceProvider {
 public:
  explicit TestMantaServiceProvider(
      std::unique_ptr<manta::MantaService> service)
      : service_(std::move(service)) {}

  manta::MantaService* Find(const AccountId& account_id) override {
    return account_id == kTestAccountId ? service_.get() : nullptr;
  }

 private:
  std::unique_ptr<manta::MantaService> service_;
};

class FakeMahiProvider : public manta::MahiProvider {
 public:
  FakeMahiProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : MahiProvider(std::move(test_url_loader_factory), identity_manager) {}

  void Summarize(const std::string& input,
                 const std::string& title,
                 const std::optional<std::string>& context,
                 const std::optional<std::string>& url,
                 manta::MantaGenericCallback callback) override {
    ++num_summarize_call_;
    latest_summary_input_ = input;
    latest_title_ = title;
    latest_url_ = url;
    std::move(callback).Run(base::DictValue().Set("outputData", kFakeSummary),
                            {manta::MantaStatusCode::kOk, "Status string ok"});
  }

  void Elucidate(const std::string& input,
                 const std::string& context,
                 const std::string& title,
                 const std::optional<std::string>& url,
                 manta::MantaGenericCallback callback) override {
    latest_elucidation_input_ = input;
    std::move(callback).Run(
        base::DictValue().Set("outputData", kFakeElucidation),
        {manta::MantaStatusCode::kOk, "Status string ok"});
  }

  // Counts the number of call to `Summarize()`
  int NumberOfSumarizeCall() { return num_summarize_call_; }

  const std::string& latest_title() const { return latest_title_; }
  const std::optional<std::string>& latest_url() const { return latest_url_; }

  const std::string& latest_elucidation_input() const {
    return latest_elucidation_input_;
  }

  const std::string& latest_summary_input() const {
    return latest_summary_input_;
  }

 private:
  int num_summarize_call_ = 0;
  std::string latest_title_;
  std::string latest_elucidation_input_;
  std::string latest_summary_input_;
  std::optional<std::string> latest_url_;
};

bool IsMahiNudgeShown() {
  return ash::Shell::Get()->anchored_nudge_manager()->IsNudgeShown(
      ash::mahi_constants::kMahiNudgeId);
}

}  // namespace

namespace ash {

class MahiManagerImplTest : public AshTestBase {
 public:
  MahiManagerImplTest()
      : AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))) {}

  MahiManagerImplTest(const MahiManagerImplTest&) = delete;
  MahiManagerImplTest& operator=(const MahiManagerImplTest&) = delete;

  ~MahiManagerImplTest() override = default;

  // AshTestBase:
  void SetUpInitParams(ash::AshTestHelper::InitParams& init_params) override {
    AshTestBase::SetUpInitParams(init_params);
    init_params.start_session = false;
  }

  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kFeatureManagementMahi},
        /*disabled_features=*/{});
    AshTestBase::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kMahiRestrictionsOverride);

    magic_boost_state_ = std::make_unique<MagicBoostState>(
        base::BindRepeating([]() { return static_cast<Profile*>(nullptr); }));
    mahi_manager_impl_ = std::make_unique<MahiManagerImpl>();
    mahi_manager_impl_->mahi_provider_ = CreateMahiProvider();

    SimulateUserLogin(kRegularUserLoginInfo);
  }

  void TearDown() override {
    mahi_manager_impl_.reset();
    magic_boost_state_.reset();
    manta_service_provider_.reset();
    identity_manager_provider_.reset();

    if (test_variations_service_) {
      TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
      test_variations_service_.reset();
    }
    metrics_state_manager_.reset();

    AshTestBase::TearDown();
  }

  void SetCountryCode(std::string_view country_code) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetString(
        variations::prefs::kVariationsCountry, country_code);
  }

  void SetUpRealEligibilityEnvironment(std::optional<bool> can_use_manta,
                                       std::string_view country_code = "us") {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        chromeos::switches::kMahiRestrictionsOverride);

    mahi_manager_impl_.reset();
    magic_boost_state_.reset();

    PrefService* local_state =
        TestingBrowserProcess::GetGlobal()->local_state();
    SetCountryCode(country_code);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state, &metrics_enabled_state_provider_,
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);
    test_variations_service_ =
        std::make_unique<variations::TestVariationsService>(
            local_state, metrics_state_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetVariationsService(
        test_variations_service_.get());

    auto& user_session_test_env =
        ash_test_helper()->user_session_test_environment();
    CHECK(user_session_test_env.AddRegularUser(kTestAccountId));
    user_session_test_env.LogIn(kTestAccountId);

    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
    if (can_use_manta.has_value()) {
      AccountCapabilitiesTestMutator mutator(&account_info);
      mutator.set_can_use_manta_service(*can_use_manta);
      identity_test_env_.UpdateAccountInfoForAccount(account_info);
    }

    identity_manager_provider_ =
        std::make_unique<FakeIdentityManagerProvider>();
    identity_manager_provider_->SetIdentityManagerForAccount(
        kTestAccountId, identity_test_env_.identity_manager());

    manta_service_provider_ = std::make_unique<TestMantaServiceProvider>(
        std::make_unique<manta::MantaService>(
            test_url_loader_factory_.GetSafeWeakWrapper(),
            identity_test_env_.identity_manager(),
            /*is_demo_mode=*/false,
            /*is_otr_profile=*/false,
            /*chrome_version=*/std::string(),
            /*chrome_channel=*/version_info::Channel::DEFAULT,
            /*locale=*/std::string()));

    // Recreate `magic_boost_state_` and `mahi_manager_impl_` so they initialize
    // against the non-overridden eligibility environment.
    magic_boost_state_ = std::make_unique<MagicBoostState>(
        base::BindRepeating([]() { return static_cast<Profile*>(nullptr); }));
    mahi_manager_impl_ = std::make_unique<MahiManagerImpl>();
    mahi_manager_impl_->mahi_provider_ = CreateMahiProvider();
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

  chromeos::MahiPageInfo CreatePageInfo(const std::string& url,
                                        const std::u16string& title,
                                        bool is_incognito = false) {
    chromeos::MahiPageInfo page_info;
    page_info.url = GURL(url);
    page_info.title = title;
    page_info.is_distillable = true;
    page_info.is_incognito = is_incognito;
    return page_info;
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

  void UpdateCurrentSelectedText() {
    mahi_manager_impl_->UpdateCurrentSelectedText();
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
  std::unique_ptr<MagicBoostState> magic_boost_state_;
  std::unique_ptr<MahiManagerImpl> mahi_manager_impl_;
  base::test::ScopedFeatureList feature_list_;

  testing::StrictMock<MockMahiMediaAppContentManager>
      mock_mahi_media_app_content_manager_;
  chromeos::ScopedMahiMediaAppContentManagerSetter
      scoped_mahi_media_app_content_manager_{
          &mock_mahi_media_app_content_manager_};

 private:
  mahi::FakeMahiWebContentsManager fake_mahi_web_contents_manager_;
  chromeos::ScopedMahiWebContentsManagerOverride
      scoped_mahi_web_contents_manager_{&fake_mahi_web_contents_manager_};

  metrics::TestEnabledStateProvider metrics_enabled_state_provider_{
      /*consent=*/false, /*enabled=*/false};
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> test_variations_service_;
  std::unique_ptr<FakeIdentityManagerProvider> identity_manager_provider_;
  std::unique_ptr<TestMantaServiceProvider> manta_service_provider_;
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

// Title and url are included in the request proto.
TEST_F(MahiManagerImplTest, SendingTitleAndUrl) {
  RequestSummary();

  EXPECT_EQ(GetMahiProvider()->latest_title(), "Title of url1");
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
    SimulateUserLogin(AccountId::FromUserEmail("other@user.test"));
    SetMahiEnabledByUserPref(!mahi_enabled);
    EXPECT_EQ(IsEnabled(), !mahi_enabled);

    // Switching back to the previous user will update to correct pref.
    SwitchActiveUser(user1_account_id);
    EXPECT_EQ(IsEnabled(), mahi_enabled);

    // Clears all logins and re-logins the default user.
    ClearLogin();
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

// Tests that `GetElucidation` can send request to and get response from mahi
// provider.
TEST_F(MahiManagerImplTest, GetElucidation) {
  // `GetElucidation` requires non empty selected text from mahi web contents
  // manager.
  const std::u16string selected_text = u"test selected text";
  chromeos::MahiWebContentsManager::Get()->SetSelectedText(selected_text);

  base::test::TestFuture<std::u16string, chromeos::MahiResponseStatus>
      test_future;

  UpdateCurrentSelectedText();
  mahi_manager_impl_->GetElucidation(test_future.GetCallback());

  // Checks mahi provider receives the request.
  EXPECT_EQ(GetMahiProvider()->latest_elucidation_input(),
            base::UTF16ToUTF8(selected_text));
  // Checks the elucidation result.
  EXPECT_EQ(test_future.Get<std::u16string>(),
            base::UTF8ToUTF16(std::string(kFakeElucidation)));
}

// Similar to `GetElucidation` above but verifies MahiManager requests content
// and selected text from media app content manager when media app is focused.
TEST_F(MahiManagerImplTest, GetElucidationForMediaApp) {
  std::string selected_text = "test PDF selected text";
  EXPECT_CALL(mock_mahi_media_app_content_manager_, GetSelectedText)
      .WillOnce(Return(selected_text));

  EXPECT_CALL(mock_mahi_media_app_content_manager_, GetFileName(_))
      .WillOnce(Return("test PDF file name"));

  chromeos::MahiPageContent page_content;
  page_content.client_id = base::UnguessableToken::Create();
  page_content.page_id = base::UnguessableToken::Create();
  page_content.page_content = u"test PDF content";

  EXPECT_CALL(mock_mahi_media_app_content_manager_, GetContent(_, _))
      .WillOnce(RunOnceCallback<1>(std::move(page_content)));

  mahi_manager_impl_->SetMediaAppPDFFocused();

  base::test::TestFuture<std::u16string, chromeos::MahiResponseStatus>
      test_future;

  UpdateCurrentSelectedText();
  mahi_manager_impl_->GetElucidation(test_future.GetCallback());

  // Checks mahi provider receives the request.
  EXPECT_EQ(GetMahiProvider()->latest_elucidation_input(), selected_text);
  // Checks the elucidation result.
  EXPECT_EQ(test_future.Get<std::u16string>(),
            base::UTF8ToUTF16(std::string(kFakeElucidation)));
}

// Tests that `GetSummary` uses `current_selected_text_` as content if it's not
// nullopt.
TEST_F(MahiManagerImplTest, GetSummaryForSelectedText) {
  const std::u16string selected_text = u"test selected text";
  chromeos::MahiWebContentsManager::Get()->SetSelectedText(selected_text);

  base::test::TestFuture<std::u16string, chromeos::MahiResponseStatus>
      test_future;

  // Sets a valid URL so that the cache manager won't drop the content.
  mahi_manager_impl_->SetCurrentFocusedPageInfo(
      CreatePageInfo("http://url1.com/abc#random", /*title=*/u"Title of url1",
                     /*is_incognito=*/false));
  UpdateCurrentSelectedText();
  mahi_manager_impl_->GetSummary(test_future.GetCallback());

  // Checks the request to mahi provider uses the selected text as input.
  EXPECT_EQ(GetMahiProvider()->latest_summary_input(),
            base::UTF16ToUTF8(selected_text));
  // Checks the summary result.
  EXPECT_EQ(test_future.Get<std::u16string>(),
            base::UTF8ToUTF16(std::string(kFakeSummary)));

  // Checks the page content is request and cached even if the summary is for
  // selected text.
  EXPECT_EQ(GetCacheManager()->size(), 1);
}

TEST_F(MahiManagerImplTest, IsEnabled_ReturnsFalseWhenMantaCapabilityUnknown) {
  SetUpRealEligibilityEnvironment(/*can_use_manta=*/std::nullopt);
  SetMahiEnabledByUserPref(true);
  EXPECT_FALSE(IsEnabled());
}

TEST_F(MahiManagerImplTest, IsEnabled_ReturnsFalseForMinorAccount) {
  SetUpRealEligibilityEnvironment(/*can_use_manta=*/false);
  SetMahiEnabledByUserPref(true);
  EXPECT_FALSE(IsEnabled());
}

TEST_F(MahiManagerImplTest,
       IsEnabled_ReturnsTrueForAdultInAllowedCountryWhenPrefEnabled) {
  SetUpRealEligibilityEnvironment(/*can_use_manta=*/true,
                                  /*country_code=*/"us");
  SetMahiEnabledByUserPref(true);
  EXPECT_TRUE(IsEnabled());

  SetMahiEnabledByUserPref(false);
  EXPECT_FALSE(IsEnabled());
}

TEST_F(MahiManagerImplTest, IsEnabled_ReturnsFalseInDisallowedCountry) {
  SetUpRealEligibilityEnvironment(/*can_use_manta=*/true,
                                  /*country_code=*/"cn");
  SetMahiEnabledByUserPref(true);
  EXPECT_FALSE(IsEnabled());
}

class MahiManagerImplDeathTest : public MahiManagerImplTest {
 protected:
  void SetUp() override {
    GTEST_FLAG_SET(death_test_style, "fast");
    MahiManagerImplTest::SetUp();
  }

 private:
  base::debug::OverrideStackTraceOutputForTesting suppress_stack_traces_{
      base::debug::OverrideStackTraceOutputForTesting::Mode::kSuppressOutput};
};

TEST_F(MahiManagerImplDeathTest,
       OnContextMenuClicked_CrashesWhenUserNotEligible) {
  SetUpRealEligibilityEnvironment(/*can_use_manta=*/false);
  ASSERT_FALSE(
      chromeos::MagicBoostState::Get()->IsUserEligibleForGenAIFeatures());

  chromeos::MahiContextMenuRequest request;
  request.action_type = chromeos::MahiActionType::kSummary;
  EXPECT_CHECK_DEATH(mahi_manager_impl_->OnContextMenuClicked(request));
}

}  // namespace ash
