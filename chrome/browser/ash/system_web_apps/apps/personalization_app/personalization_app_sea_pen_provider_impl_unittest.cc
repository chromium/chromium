// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_impl.h"

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/manta/features.h"
#include "components/manta/manta_status.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::personalization_app {

namespace {

using std::literals::string_view_literals::operator""sv;

constexpr char kFakeTestEmail[] = "fakeemail@personalization";
constexpr char kTestGaiaId[] = "1234567890";
constexpr char kFakeTestEmail2[] = "anotherfakeemail@personalization";
constexpr char kTestGaiaId2[] = "9876543210";
constexpr char kGooglerEmail[] = "user@google.com";
constexpr char kGooglerGaiaId[] = "123459876";

// Repeat `string_view` until the output is size `target_size` or as close as
// possible to `target_size` without being longer.
std::string RepeatToSize(std::string_view repeat,
                         std::string::size_type target_size) {
  auto repeat_size = repeat.size();
  int i = 1;
  std::stringstream ss;
  while ((repeat_size * i) <= target_size) {
    ss << repeat;
    i++;
  }
  return ss.str();
}

AccountId GetTestAccountId() {
  return AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId);
}

AccountId GetTestAccountId2() {
  return AccountId::FromUserEmailGaiaId(kFakeTestEmail2, kTestGaiaId2);
}

AccountId GetGooglerAccountId() {
  return AccountId::FromUserEmailGaiaId(kGooglerEmail, kGooglerGaiaId);
}

void AddAndLoginUser(const AccountId& account_id, user_manager::UserType type) {
  user_manager::User* user = nullptr;
  ash::FakeChromeUserManager* user_manager =
      static_cast<ash::FakeChromeUserManager*>(
          user_manager::UserManager::Get());
  switch (type) {
    case user_manager::UserType ::kRegular:
      user = user_manager->AddUser(account_id);
      break;
    case user_manager::UserType::kGuest:
      user = user_manager->AddGuestUser();
      break;
    case user_manager::UserType::kChild:
      user = user_manager->AddChildUser(account_id);
      break;
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kArcKioskApp:
    case user_manager::UserType::kWebKioskApp:
      break;
  }

  if (!user) {
    return;
  }

  user_manager->LoginUser(user->GetAccountId());
  user_manager->SwitchActiveUser(user->GetAccountId());
}

testing::Matcher<ash::personalization_app::mojom::SeaPenThumbnailPtr>
MatchesSeaPenImage(const std::string_view expected_jpg_bytes,
                   const uint32_t expected_id) {
  return testing::AllOf(
      testing::Pointee(testing::Field(
          &ash::personalization_app::mojom::SeaPenThumbnail::image,
          GetJpegDataUrl(expected_jpg_bytes))),
      testing::Pointee(testing::Field(
          &ash::personalization_app::mojom::SeaPenThumbnail::id, expected_id)));
}

base::subtle::ScopedTimeClockOverrides CreateScopedTimeNowOverride() {
  return base::subtle::ScopedTimeClockOverrides(
      []() -> base::Time {
        base::Time fake_now;
        bool success =
            base::Time::FromString("2023-04-05T01:23:45Z", &fake_now);
        DCHECK(success);
        return fake_now;
      },
      nullptr, nullptr);
}

class PersonalizationAppSeaPenProviderImplTest : public testing::Test {
 public:
  PersonalizationAppSeaPenProviderImplTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures(
        {features::kSeaPen, features::kFeatureManagementSeaPen}, {});
  }

  PersonalizationAppSeaPenProviderImplTest(
      const PersonalizationAppSeaPenProviderImplTest&) = delete;
  PersonalizationAppSeaPenProviderImplTest& operator=(
      const PersonalizationAppSeaPenProviderImplTest&) = delete;

  ~PersonalizationAppSeaPenProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
  }

  // Set up the profile for an account. This can be used to set up the profile
  // again with the new account when switching between accounts.
  void SetUpProfileForTesting(
      const std::string& name,
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::UserType::kRegular) {
    AddProfile(name, user_type);
    AddAndLoginUser(account_id, user_type);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    sea_pen_provider_ = std::make_unique<PersonalizationAppSeaPenProviderImpl>(
        &web_ui_,
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
    sea_pen_provider_remote_.reset();
    sea_pen_provider_->BindInterface(
        sea_pen_provider_remote_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<ash::personalization_app::mojom::SeaPenProvider>&
  sea_pen_provider_remote() {
    return sea_pen_provider_remote_;
  }

  TestWallpaperController* test_wallpaper_controller() {
    return &test_wallpaper_controller_;
  }

  PersonalizationAppSeaPenProviderImpl* sea_pen_provider() {
    return sea_pen_provider_.get();
  }

  TestingProfile* profile() { return profile_; }

 private:
  void AddProfile(const std::string& name, user_manager::UserType user_type) {
    switch (user_type) {
      case user_manager::UserType::kRegular:
        profile_ = profile_manager_.CreateTestingProfile(name);
        break;
      case user_manager::UserType::kChild:
        profile_ = profile_manager_.CreateTestingProfile(name);
        profile_->SetIsSupervisedProfile(true);
        break;
      case user_manager::UserType::kGuest:
        profile_ = profile_manager_.CreateGuestProfile();
        break;
      case user_manager::UserType::kPublicAccount:
      case user_manager::UserType::kKioskApp:
      case user_manager::UserType::kArcKioskApp:
      case user_manager::UserType::kWebKioskApp:
        profile_ = profile_manager_.CreateTestingProfile(name);
        break;
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestWallpaperController test_wallpaper_controller_;
  content::TestWebUI web_ui_;
  user_manager::ScopedUserManager scoped_user_manager_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  mojo::Remote<ash::personalization_app::mojom::SeaPenProvider>
      sea_pen_provider_remote_;
  std::unique_ptr<PersonalizationAppSeaPenProviderImpl> sea_pen_provider_;
};

TEST_F(PersonalizationAppSeaPenProviderImplTest, TextSearchReturnsThumbnails) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;
  mojom::SeaPenQueryPtr search_query =
      mojom::SeaPenQuery::NewTextQuery("search_query");

  sea_pen_provider_remote()->SearchWallpaper(
      std::move(search_query), search_wallpaper_future.GetCallback());

  EXPECT_THAT(
      search_wallpaper_future.Get<0>().value(),
      testing::ElementsAre(MatchesSeaPenImage("fake_sea_pen_image_1"sv, 1),
                           MatchesSeaPenImage("fake_sea_pen_image_2"sv, 2),
                           MatchesSeaPenImage("fake_sea_pen_image_3"sv, 3),
                           MatchesSeaPenImage("fake_sea_pen_image_4"sv, 4)));
  EXPECT_EQ(search_wallpaper_future.Get<1>(), manta::MantaStatusCode::kOk);
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       TemplateSearchReturnsThumbnails) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;
  base::flat_map<mojom::SeaPenTemplateChip, mojom::SeaPenTemplateOption>
      options({{mojom::SeaPenTemplateChip::kFlowerColor,
                mojom::SeaPenTemplateOption::kFlowerColorBlue},
               {mojom::SeaPenTemplateChip::kFlowerType,
                mojom::SeaPenTemplateOption::kFlowerTypeRose}});
  mojom::SeaPenQueryPtr search_query =
      mojom::SeaPenQuery::NewTemplateQuery(mojom::SeaPenTemplateQuery::New(
          mojom::SeaPenTemplateId::kFlower, options,
          mojom::SeaPenUserVisibleQuery::New("test template query",
                                             "test template title")));

  sea_pen_provider_remote()->SearchWallpaper(
      std::move(search_query), search_wallpaper_future.GetCallback());

  EXPECT_THAT(
      search_wallpaper_future.Get<0>().value(),
      testing::ElementsAre(MatchesSeaPenImage("fake_sea_pen_image_1"sv, 1),
                           MatchesSeaPenImage("fake_sea_pen_image_2"sv, 2),
                           MatchesSeaPenImage("fake_sea_pen_image_3"sv, 3),
                           MatchesSeaPenImage("fake_sea_pen_image_4"sv, 4)));
  EXPECT_THAT(search_wallpaper_future.Get<1>(),
              testing::Eq(manta::MantaStatusCode::kOk));
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, MaxLengthQuery) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  // "\uFFFF" is picked because `.size()` differs by a factor of three
  // between UTF-8 (C++ std::string) and UTF-16 (javascript string).
  std::string long_unicode_string =
      RepeatToSize("\uFFFF", mojom::kMaximumSearchWallpaperTextBytes);
  ASSERT_EQ(mojom::kMaximumSearchWallpaperTextBytes,
            long_unicode_string.size());
  // In javascript UTF-16, `long_unicode_string.length` is 1/3.
  ASSERT_EQ(mojom::kMaximumSearchWallpaperTextBytes / 3,
            base::UTF8ToUTF16(long_unicode_string).size());

  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;
  mojom::SeaPenQueryPtr long_query =
      mojom::SeaPenQuery::NewTextQuery(long_unicode_string);

  sea_pen_provider_remote()->SearchWallpaper(
      std::move(long_query), search_wallpaper_future.GetCallback());

  EXPECT_EQ(4u, search_wallpaper_future.Get<0>().value().size())
      << "SearchWallpaper succeeds if text is exactly max length";
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, QueryLengthExceeded) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  std::string max_length_unicode_string =
      RepeatToSize("\uFFFF", mojom::kMaximumSearchWallpaperTextBytes);
  mojom::SeaPenQueryPtr bad_long_query =
      mojom::SeaPenQuery::NewTextQuery(max_length_unicode_string + 'a');
  mojo::test::BadMessageObserver bad_message_observer;

  sea_pen_provider_remote()->SearchWallpaper(
      std::move(bad_long_query),
      base::BindLambdaForTesting(
          [](std::optional<std::vector<
                 ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
             manta::MantaStatusCode) { NOTREACHED(); }));

  EXPECT_EQ("SearchWallpaper exceeded maximum text length",
            bad_message_observer.WaitForBadMessage())
      << "SearchWallpaper fails if text is longer than max length";
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       SelectThumbnailSetsSeaPenWallpaper) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  // Store some test images in the provider so that one can be selected.
  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;
  mojom::SeaPenQueryPtr search_query =
      mojom::SeaPenQuery::NewTextQuery("search_query");
  sea_pen_provider_remote()->SearchWallpaper(
      std::move(search_query), search_wallpaper_future.GetCallback());

  ASSERT_EQ(0, test_wallpaper_controller()->get_sea_pen_wallpaper_count());
  ASSERT_FALSE(test_wallpaper_controller()->wallpaper_info().has_value());

  // Select the first returned thumbnail.
  base::test::TestFuture<bool> select_wallpaper_future;
  sea_pen_provider_remote()->SelectSeaPenThumbnail(
      search_wallpaper_future.Get<0>().value().front()->id,
      select_wallpaper_future.GetCallback());

  ASSERT_TRUE(select_wallpaper_future.Take());
  EXPECT_EQ(1, test_wallpaper_controller()->get_sea_pen_wallpaper_count());
  EXPECT_EQ(WallpaperType::kSeaPen,
            test_wallpaper_controller()->wallpaper_info()->type);
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, GetRecentSeaPenImages) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  auto chromeos_wallpaper_dir_override_ =
      std::make_unique<base::ScopedPathOverride>(
          chrome::DIR_CHROMEOS_WALLPAPERS, scoped_temp_dir.GetPath());

  base::FilePath sea_pen_dir1 =
      scoped_temp_dir.GetPath().Append("sea_pen").Append(
          GetTestAccountId().GetAccountIdKey());
  ASSERT_TRUE(base::CreateDirectory(sea_pen_dir1));
  base::FilePath sea_pen_file_path1 = sea_pen_dir1.Append("111.jpg");
  ASSERT_TRUE(base::WriteFile(sea_pen_file_path1, "test image 1"));
  base::FilePath sea_pen_file_path2 = sea_pen_dir1.Append("222.jpg");
  ASSERT_TRUE(base::WriteFile(sea_pen_file_path2, "test image 2"));

  base::test::TestFuture<const std::vector<base::FilePath>&>
      recent_images_future;
  sea_pen_provider_remote()->GetRecentSeaPenImages(
      recent_images_future.GetCallback());

  std::vector<base::FilePath> recent_images = recent_images_future.Take();
  ASSERT_EQ(2u, recent_images.size());
  EXPECT_TRUE(base::Contains(recent_images, sea_pen_file_path1));
  EXPECT_TRUE(base::Contains(recent_images, sea_pen_file_path2));

  // Log in the second user, get the list of recent images.
  SetUpProfileForTesting(kFakeTestEmail2, GetTestAccountId2());
  sea_pen_provider_remote()->GetRecentSeaPenImages(
      recent_images_future.GetCallback());
  ASSERT_EQ(0u, recent_images_future.Take().size());
  ASSERT_TRUE(base::PathExists(sea_pen_file_path1));
  ASSERT_TRUE(base::PathExists(sea_pen_file_path2));

  // Create an image in the Sea Pen directory for second user, then get the list
  // of recent images again.
  base::FilePath sea_pen_dir2 =
      scoped_temp_dir.GetPath().Append("sea_pen").Append(
          GetTestAccountId2().GetAccountIdKey());
  ASSERT_TRUE(base::CreateDirectory(sea_pen_dir2));
  base::FilePath sea_pen_file_path3 = sea_pen_dir2.Append("111.jpg");
  ASSERT_TRUE(base::WriteFile(sea_pen_file_path3, "test image 3"));

  sea_pen_provider_remote()->GetRecentSeaPenImages(
      recent_images_future.GetCallback());
  recent_images = recent_images_future.Take();
  ASSERT_EQ(1u, recent_images.size());
  EXPECT_TRUE(base::Contains(recent_images, sea_pen_file_path3));
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       SelectThumbnailSendsFreeTextQuery) {
  auto time_override = CreateScopedTimeNowOverride();

  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  std::string user_search_query = "user search query text";

  // Store some test images in the provider so that one can be selected.
  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;
  mojom::SeaPenQueryPtr search_query =
      mojom::SeaPenQuery::NewTextQuery(user_search_query);
  sea_pen_provider_remote()->SearchWallpaper(
      std::move(search_query), search_wallpaper_future.GetCallback());

  // Select the first returned thumbnail.
  base::test::TestFuture<bool> select_wallpaper_future;
  sea_pen_provider_remote()->SelectSeaPenThumbnail(
      search_wallpaper_future.Get<0>().value().front()->id,
      select_wallpaper_future.GetCallback());

  ASSERT_TRUE(select_wallpaper_future.Take());
  EXPECT_TRUE(test_wallpaper_controller()->sea_pen_query().Equals(
      mojom::SeaPenQuery::NewTextQuery(user_search_query)));
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       SelectThumbnailSendsTemplateQuery) {
  auto time_override = CreateScopedTimeNowOverride();

  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  // Store some test images in the provider so that one can be selected.
  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;

  const base::flat_map<mojom::SeaPenTemplateChip, mojom::SeaPenTemplateOption>
      chosen_options = {
          {mojom::SeaPenTemplateChip::kCharactersBackground,
           mojom::SeaPenTemplateOption::kCharactersBackgroundOlive},
          {mojom::SeaPenTemplateChip::kCharactersColor,
           mojom::SeaPenTemplateOption::kCharactersColorBeige},
          {mojom::SeaPenTemplateChip::kCharactersSubjects,
           mojom::SeaPenTemplateOption::kCharactersSubjectsBicycles}};

  mojom::SeaPenQueryPtr search_query =
      mojom::SeaPenQuery::NewTemplateQuery(mojom::SeaPenTemplateQuery::New(
          mojom::SeaPenTemplateId::kCharacters, chosen_options,
          mojom::SeaPenUserVisibleQuery::New("test template query",
                                             "test template title")));

  sea_pen_provider_remote()->SearchWallpaper(
      std::move(search_query), search_wallpaper_future.GetCallback());

  // Select the first returned thumbnail.
  base::test::TestFuture<bool> select_wallpaper_future;
  sea_pen_provider_remote()->SelectSeaPenThumbnail(
      search_wallpaper_future.Get<0>().value().front()->id,
      select_wallpaper_future.GetCallback());

  ASSERT_TRUE(select_wallpaper_future.Take());

  EXPECT_TRUE(test_wallpaper_controller()->sea_pen_query().Equals(
      mojom::SeaPenQuery::NewTemplateQuery(mojom::SeaPenTemplateQuery::New(
          mojom::SeaPenTemplateId::kCharacters, chosen_options,
          mojom::SeaPenUserVisibleQuery::New("test template query",
                                             "test template title")))));
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       ShouldShowSeaPenTermsOfServiceDialog) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  test_wallpaper_controller()->ClearCounts();
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kSeaPen}, {});

  base::test::TestFuture<bool> should_show_dialog_future;
  sea_pen_provider_remote()->ShouldShowSeaPenTermsOfServiceDialog(
      should_show_dialog_future.GetCallback());
  // Expects to return true before the terms are accepted.
  EXPECT_TRUE(should_show_dialog_future.Take());

  sea_pen_provider_remote()->HandleSeaPenTermsOfServiceAccepted();

  sea_pen_provider_remote()->ShouldShowSeaPenTermsOfServiceDialog(
      should_show_dialog_future.GetCallback());
  // Expects to return false after the terms are accepted.
  EXPECT_FALSE(should_show_dialog_future.Take());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, IsEligibleForSeaPen_Guest) {
  SetUpProfileForTesting("guest", user_manager::GuestAccountId(),
                         user_manager::UserType::kGuest);
  ASSERT_FALSE(sea_pen_provider()->IsEligibleForSeaPen());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, IsEligibleForSeaPen_Child) {
  SetUpProfileForTesting("child", GetTestAccountId(),
                         user_manager::UserType::kChild);
  ASSERT_FALSE(sea_pen_provider()->IsEligibleForSeaPen());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, IsEligibleForSeaPen_Googler) {
  // Managed Googlers can still access SeaPen.
  SetUpProfileForTesting(kGooglerEmail, GetGooglerAccountId());
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  ASSERT_TRUE(sea_pen_provider()->IsEligibleForSeaPen());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, IsEligibleForSeaPen_Managed) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  ASSERT_FALSE(sea_pen_provider()->IsEligibleForSeaPen());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, IsEligibleForSeaPen_Regular) {
  SetUpProfileForTesting(kFakeTestEmail2, GetTestAccountId2());
  ASSERT_TRUE(sea_pen_provider()->IsEligibleForSeaPen());
}

}  // namespace

}  // namespace ash::personalization_app
