// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_impl.h"

#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
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
#include "chrome/browser/ash/mahi/fake_mahi_browser_delegate_ash.h"
#include "chrome/browser/ash/mahi/mahi_cache_manager.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/constants/chromeos_features.h"
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

using ::testing::IsNull;

constexpr char kFakeSummary[] = "Fake summary";

class FakeMahiProvider : public manta::MahiProvider {
 public:
  FakeMahiProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : MahiProvider(std::move(test_url_loader_factory), identity_manager) {}

  void Summarize(const std::string& input,
                 manta::MantaGenericCallback callback) override {
    ++num_summarize_call_;
    std::move(callback).Run(base::Value::Dict().Set("outputData", kFakeSummary),
                            {manta::MantaStatusCode::kOk, "Status string ok"});
  }

  // Counts the number of call to `Summarize()`
  int NumberOfSumarizeCall() { return num_summarize_call_; }

 private:
  int num_summarize_call_ = 0;
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
    NoSessionAshTestBase::SetUp();

    magic_boost_state_ = std::make_unique<MagicBoostStateAsh>();
    mahi_manager_impl_ = std::make_unique<MahiManagerImpl>();
    mahi_manager_impl_->mahi_provider_ = CreateMahiProvider();

    fake_mahi_browser_delegate_ash_ =
        std::make_unique<FakeMahiBrowserDelegateAsh>();
    mahi_manager_impl_->mahi_browser_delegate_ash_ =
        fake_mahi_browser_delegate_ash_.get();

    CreateUserSessions(1);
  }

  void TearDown() override {
    mahi_manager_impl_.reset();
    magic_boost_state_.reset();
    fake_mahi_browser_delegate_ash_.reset();
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

  bool IsEnabled() const { return mahi_manager_impl_->IsEnabled(); }

  crosapi::mojom::MahiPageInfoPtr CreatePageInfo(const std::string& url,
                                                 const std::u16string& title) {
    return crosapi::mojom::MahiPageInfo::New(
        /*client_id=*/base::UnguessableToken(),
        /*page_id=*/base::UnguessableToken(), /*url=*/GURL(url),
        /*title=*/title,
        /*favicon_image=*/gfx::ImageSkia(), /*is_distillable*/ true);
  }

  MahiCacheManager* GetCacheManager() {
    return mahi_manager_impl_->cache_manager_.get();
  }

  void NotifyRefreshAvailability(bool available) {
    mahi_manager_impl_->NotifyRefreshAvailability(available);
  }

  void RequestSummary() {
    // Sets the page that needed to get summary.
    mahi_manager_impl_->SetCurrentFocusedPageInfo(
        CreatePageInfo("http://url1.com/abc#skip", u"Title of url1"));
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

 private:
  base::test::ScopedFeatureList feature_list_{chromeos::features::kMahi};
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<FakeMahiBrowserDelegateAsh> fake_mahi_browser_delegate_ash_;
};

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
