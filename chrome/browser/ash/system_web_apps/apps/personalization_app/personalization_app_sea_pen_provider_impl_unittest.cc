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
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "ash/wallpaper/test_sea_pen_wallpaper_manager_session_delegate.h"
#include "ash/wallpaper/wallpaper_file_manager.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/test_sea_pen_observer.h"
#include "chrome/browser/ash/wallpaper_handlers/mock_sea_pen_fetcher.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper/test_wallpaper_controller.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
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
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash::personalization_app {

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@personalization";
constexpr char kTestGaiaId[] = "1234567890";
constexpr char kFakeTestEmail2[] = "anotherfakeemail@personalization";
constexpr char kTestGaiaId2[] = "9876543210";
constexpr char kGooglerEmail[] = "user@google.com";
constexpr char kGooglerGaiaId[] = "123459876";
constexpr char kDemoModeEmail[] = "demo-public-account@example.com";

constexpr uint32_t kSeaPenId1 = 111;
constexpr uint32_t kSeaPenId2 = 222;

SkBitmap CreateBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseARGB(255, 31, 63, 127);
  return bitmap;
}

// Create fake Jpg image bytes.
std::string CreateJpgBytes() {
  SkBitmap bitmap = CreateBitmap();
  std::vector<unsigned char> data;
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/100, &data);
  return std::string(data.begin(), data.end());
}

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

AccountId GetDemoModeAccountId() {
  return AccountId::FromUserEmail(kDemoModeEmail);
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
      user = user_manager->AddPublicAccountUser(account_id);
      break;
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      break;
  }

  if (!user) {
    return;
  }

  user_manager->LoginUser(user->GetAccountId());
  user_manager->SwitchActiveUser(user->GetAccountId());
}

testing::Matcher<ash::personalization_app::mojom::SeaPenThumbnailPtr>
MatchesSeaPenImage(std::string_view expected_jpg_bytes,
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
        {features::kSeaPen, features::kSeaPenDemoMode,
         features::kFeatureManagementSeaPen},
        {});
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
    sea_pen_wallpaper_manager_.SetSessionDelegateForTesting(
        std::make_unique<TestSeaPenWallpaperManagerSessionDelegate>());
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

    SetSeaPenObserver();
  }

  TestSeaPenWallpaperManagerSessionDelegate*
  sea_pen_wallpaper_manager_session_delegate() {
    return static_cast<TestSeaPenWallpaperManagerSessionDelegate*>(
        sea_pen_wallpaper_manager_.session_delegate_for_testing());
  }

  TestSeaPenObserver& test_sea_pen_observer() { return test_sea_pen_observer_; }

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

  void SetSeaPenObserver() {
    sea_pen_provider_remote_->SetSeaPenObserver(
        test_sea_pen_observer_.GetPendingRemote());
  }

  void CreateSeaPenFilesForTesting(const AccountId& account_id,
                                   std::vector<uint32_t> sea_pen_ids) {
    for (const uint32_t& sea_pen_id : sea_pen_ids) {
      base::test::TestFuture<bool> save_sea_pen_image_future;
      sea_pen_wallpaper_manager_.SaveSeaPenImage(
          account_id, {CreateJpgBytes(), sea_pen_id},
          personalization_app::mojom::SeaPenQuery::NewTextQuery(
              "test query " + base::NumberToString(sea_pen_id)),
          save_sea_pen_image_future.GetCallback());
      ASSERT_TRUE(save_sea_pen_image_future.Get());
    }
  }

  void SetSeaPenFetcherResponse(
      std::vector<uint32_t> image_ids,
      manta::MantaStatusCode status_code,
      const ash::personalization_app::mojom::SeaPenQueryPtr& expected_query) {
    std::vector<SeaPenImage> images;
    for (const auto image_id : image_ids) {
      images.emplace_back(CreateJpgBytes(), image_id);
    }

    auto* fetcher =
        static_cast<testing::NiceMock<wallpaper_handlers::MockSeaPenFetcher>*>(
            sea_pen_provider_->GetOrCreateSeaPenFetcher());
    EXPECT_CALL(*fetcher, FetchThumbnails)
        .WillOnce(
            [inner_status_code = status_code,
             &inner_expected_query = expected_query,
             inner_images = std::move(images)](
                manta::proto::FeatureName feature_name,
                const ash::personalization_app::mojom::SeaPenQueryPtr& query,
                wallpaper_handlers::SeaPenFetcher::OnFetchThumbnailsComplete
                    callback) mutable {
              EXPECT_EQ(manta::proto::FeatureName::CHROMEOS_WALLPAPER,
                        feature_name);
              EXPECT_EQ(inner_expected_query, query);
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback), std::move(inner_images),
                                 inner_status_code));
            });
  }

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
      case user_manager::UserType::kWebKioskApp:
      case user_manager::UserType::kKioskIWA:
        profile_ = profile_manager_.CreateTestingProfile(name);
        break;
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestWallpaperController test_wallpaper_controller_;
  SeaPenWallpaperManager sea_pen_wallpaper_manager_;
  content::TestWebUI web_ui_;
  InProcessDataDecoder in_process_data_decoder_;
  user_manager::ScopedUserManager scoped_user_manager_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  mojo::Remote<ash::personalization_app::mojom::SeaPenProvider>
      sea_pen_provider_remote_;
  std::unique_ptr<PersonalizationAppSeaPenProviderImpl> sea_pen_provider_;
  TestSeaPenObserver test_sea_pen_observer_;
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

  sea_pen_provider_remote()->GetSeaPenThumbnails(
      std::move(search_query), search_wallpaper_future.GetCallback());

  EXPECT_THAT(
      search_wallpaper_future.Get<0>().value(),
      testing::ElementsAre(MatchesSeaPenImage("fake_sea_pen_image_1", 1),
                           MatchesSeaPenImage("fake_sea_pen_image_2", 2),
                           MatchesSeaPenImage("fake_sea_pen_image_3", 3),
                           MatchesSeaPenImage("fake_sea_pen_image_4", 4)));
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

  sea_pen_provider_remote()->GetSeaPenThumbnails(
      std::move(search_query), search_wallpaper_future.GetCallback());

  EXPECT_THAT(
      search_wallpaper_future.Get<0>().value(),
      testing::ElementsAre(MatchesSeaPenImage("fake_sea_pen_image_1", 1),
                           MatchesSeaPenImage("fake_sea_pen_image_2", 2),
                           MatchesSeaPenImage("fake_sea_pen_image_3", 3),
                           MatchesSeaPenImage("fake_sea_pen_image_4", 4)));
  EXPECT_THAT(search_wallpaper_future.Get<1>(),
              testing::Eq(manta::MantaStatusCode::kOk));
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, MaxLengthQuery) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  // "\uFFFF" is picked because `.size()` differs by a factor of three
  // between UTF-8 (C++ std::string) and UTF-16 (javascript string).
  std::string long_unicode_string =
      RepeatToSize("\uFFFF", mojom::kMaximumGetSeaPenThumbnailsTextBytes);
  ASSERT_EQ(mojom::kMaximumGetSeaPenThumbnailsTextBytes,
            long_unicode_string.size());
  // In javascript UTF-16, `long_unicode_string.length` is 1/3.
  ASSERT_EQ(mojom::kMaximumGetSeaPenThumbnailsTextBytes / 3,
            base::UTF8ToUTF16(long_unicode_string).size());

  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;
  mojom::SeaPenQueryPtr long_query =
      mojom::SeaPenQuery::NewTextQuery(long_unicode_string);

  sea_pen_provider_remote()->GetSeaPenThumbnails(
      std::move(long_query), search_wallpaper_future.GetCallback());

  EXPECT_EQ(4u, search_wallpaper_future.Get<0>().value().size())
      << "GetSeaPenThumbnails succeeds if text is exactly max length";
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, QueryLengthExceeded) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  std::string max_length_unicode_string =
      RepeatToSize("\uFFFF", mojom::kMaximumGetSeaPenThumbnailsTextBytes);
  mojom::SeaPenQueryPtr bad_long_query =
      mojom::SeaPenQuery::NewTextQuery(max_length_unicode_string + 'a');
  mojo::test::BadMessageObserver bad_message_observer;

  sea_pen_provider_remote()->GetSeaPenThumbnails(
      std::move(bad_long_query),
      base::BindLambdaForTesting(
          [](std::optional<std::vector<
                 ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
             manta::MantaStatusCode) { NOTREACHED_IN_MIGRATION(); }));

  EXPECT_EQ("GetSeaPenThumbnails exceeded maximum text length",
            bad_message_observer.WaitForBadMessage())
      << "GetSeaPenThumbnails fails if text is longer than max length";
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       SelectThumbnailSetsSeaPenWallpaper) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());

  auto query = mojom::SeaPenQuery::NewTextQuery("search_query");

  // Send real images that will pass decoding.
  SetSeaPenFetcherResponse({963, 246}, manta::MantaStatusCode::kOk, query);

  // Store the above test images in the provider so that one can be selected.
  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;

  sea_pen_provider_remote()->GetSeaPenThumbnails(
      query->Clone(), search_wallpaper_future.GetCallback());

  ASSERT_EQ(963u, search_wallpaper_future.Get<0>().value().front()->id);
  ASSERT_EQ(manta::MantaStatusCode::kOk,
            search_wallpaper_future.Get<manta::MantaStatusCode>());

  ASSERT_EQ(0, test_wallpaper_controller()->get_sea_pen_wallpaper_count());
  ASSERT_FALSE(test_wallpaper_controller()->wallpaper_info().has_value());

  // Select the first returned thumbnail.
  base::test::TestFuture<bool> select_wallpaper_future;
  sea_pen_provider_remote()->SelectSeaPenThumbnail(
      search_wallpaper_future.Get<0>().value().front()->id,
      /*preview_mode=*/false, select_wallpaper_future.GetCallback());

  ASSERT_TRUE(select_wallpaper_future.Take());
  EXPECT_EQ(1, test_wallpaper_controller()->get_sea_pen_wallpaper_count());
  EXPECT_EQ(WallpaperType::kSeaPen,
            test_wallpaper_controller()->wallpaper_info()->type);
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, SelectThumbnailCallsObserver) {
  constexpr uint32_t kIdToSelect = 963;

  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  test_wallpaper_controller()->SetCurrentUser(GetTestAccountId());

  // Set some other wallpaper type.
  test_wallpaper_controller()->SetOnlineWallpaper(
      {GetTestAccountId(),
       "collection_id",
       WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false,
       /*from_user=*/true,
       /*daily_refresh_enabled=*/false,
       /*unit_id=*/1u,
       {{/*asset_id=*/1u, /*raw_url=*/GURL("http://test_url"),
         backdrop::Image::IMAGE_TYPE_UNKNOWN}}},
      base::DoNothing());

  base::test::TestFuture<std::optional<uint32_t>> initial_id_future;
  test_sea_pen_observer().SetCallback(initial_id_future.GetCallback());

  // No SeaPen wallpaper set yet. But should still update the observer after it
  // is first bound.
  ASSERT_FALSE(initial_id_future.Get().has_value());
  ASSERT_EQ(1u, test_sea_pen_observer().id_updated_count());

  ASSERT_FALSE(test_sea_pen_observer().GetCurrentId().has_value());

  auto query = mojom::SeaPenQuery::NewTextQuery("search_query");

  // Send real images that will pass decoding.
  SetSeaPenFetcherResponse({kIdToSelect, 246}, manta::MantaStatusCode::kOk,
                           query);

  base::test::TestFuture<std::optional<uint32_t>> sea_pen_id_future;
  test_sea_pen_observer().SetCallback(sea_pen_id_future.GetCallback());

  // Store the above test images in the provider so that one can be selected.
  sea_pen_provider_remote()->GetSeaPenThumbnails(query->Clone(),
                                                 base::DoNothing());

  // Select the first returned thumbnail.
  sea_pen_provider_remote()->SelectSeaPenThumbnail(
      kIdToSelect, /*preview_mode=*/false,
      base::BindLambdaForTesting(
          [test_wallpaper_controller =
               test_wallpaper_controller()](bool success) {
            ASSERT_TRUE(success);
            // Simulate a wallpaper being set to notify observers.
            test_wallpaper_controller->ShowWallpaperImage(
                gfx::test::CreateImageSkia(1, 1));
          }));

  EXPECT_EQ(kIdToSelect, sea_pen_id_future.Get());
  EXPECT_EQ(kIdToSelect, test_sea_pen_observer().GetCurrentId().value());
  EXPECT_EQ(2u, test_sea_pen_observer().id_updated_count());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       GetTextQueryThumbnailsCallsObserver) {
  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;

  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  test_wallpaper_controller()->SetCurrentUser(GetTestAccountId());

  auto query = mojom::SeaPenQuery::NewTextQuery("search_query");
  SetSeaPenFetcherResponse({246}, manta::MantaStatusCode::kOk, query);
  sea_pen_provider_remote()->GetSeaPenThumbnails(
      query->Clone(), search_wallpaper_future.GetCallback());
  ASSERT_EQ(246u, search_wallpaper_future.Get<0>().value().front()->id);
  search_wallpaper_future.Clear();
  EXPECT_TRUE(test_sea_pen_observer().GetHistoryEntries()->empty());

  query = mojom::SeaPenQuery::NewTextQuery("search_query_1");
  SetSeaPenFetcherResponse({247}, manta::MantaStatusCode::kOk, query);
  sea_pen_provider_remote()->GetSeaPenThumbnails(
      query->Clone(), search_wallpaper_future.GetCallback());
  ASSERT_EQ(247u, search_wallpaper_future.Get<0>().value().front()->id);
  search_wallpaper_future.Clear();

  query = mojom::SeaPenQuery::NewTextQuery("search_query_2");
  SetSeaPenFetcherResponse({248}, manta::MantaStatusCode::kOk, query);
  sea_pen_provider_remote()->GetSeaPenThumbnails(
      query->Clone(), search_wallpaper_future.GetCallback());
  ASSERT_EQ(248u, search_wallpaper_future.Get<0>().value().front()->id);
  search_wallpaper_future.Clear();

  query = mojom::SeaPenQuery::NewTextQuery("search_query_3");
  SetSeaPenFetcherResponse({249}, manta::MantaStatusCode::kOk, query);
  sea_pen_provider_remote()->GetSeaPenThumbnails(
      query->Clone(), search_wallpaper_future.GetCallback());
  ASSERT_EQ(249u, search_wallpaper_future.Get<0>().value().front()->id);
  search_wallpaper_future.Clear();

  query = mojom::SeaPenQuery::NewTextQuery("search_query_4");
  SetSeaPenFetcherResponse({250}, manta::MantaStatusCode::kOk, query);
  sea_pen_provider_remote()->GetSeaPenThumbnails(
      query->Clone(), search_wallpaper_future.GetCallback());
  ASSERT_EQ(250u, search_wallpaper_future.Get<0>().value().front()->id);
  search_wallpaper_future.Clear();

  auto history = test_sea_pen_observer().GetHistoryEntries();
  EXPECT_EQ(3u, history->size());
  EXPECT_EQ("search_query_3", history->at(0)->query);
  EXPECT_THAT(history->at(0)->thumbnails,
              testing::UnorderedElementsAre(
                  testing::Pointee(testing::FieldsAre(testing::_, 249))));
  EXPECT_EQ("search_query_2", history->at(1)->query);
  EXPECT_THAT(history->at(1)->thumbnails,
              testing::UnorderedElementsAre(
                  testing::Pointee(testing::FieldsAre(testing::_, 248))));
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       SelectThumbnailFromTextQueryHistory) {
  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;

  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  test_wallpaper_controller()->SetCurrentUser(GetTestAccountId());

  auto query = mojom::SeaPenQuery::NewTextQuery("search_query");
  SetSeaPenFetcherResponse({246}, manta::MantaStatusCode::kOk, query);
  sea_pen_provider_remote()->GetSeaPenThumbnails(
      query->Clone(), search_wallpaper_future.GetCallback());
  ASSERT_EQ(246u, search_wallpaper_future.Get<0>().value().front()->id);
  search_wallpaper_future.Clear();
  EXPECT_TRUE(test_sea_pen_observer().GetHistoryEntries()->empty());

  query = mojom::SeaPenQuery::NewTextQuery("search_query_1");
  SetSeaPenFetcherResponse({247}, manta::MantaStatusCode::kOk, query);
  sea_pen_provider_remote()->GetSeaPenThumbnails(
      query->Clone(), search_wallpaper_future.GetCallback());
  ASSERT_EQ(247u, search_wallpaper_future.Get<0>().value().front()->id);
  search_wallpaper_future.Clear();

  query = mojom::SeaPenQuery::NewTextQuery("search_query_2");
  SetSeaPenFetcherResponse({248}, manta::MantaStatusCode::kOk, query);
  sea_pen_provider_remote()->GetSeaPenThumbnails(
      query->Clone(), search_wallpaper_future.GetCallback());
  ASSERT_EQ(248u, search_wallpaper_future.Get<0>().value().front()->id);
  search_wallpaper_future.Clear();

  query = mojom::SeaPenQuery::NewTextQuery("search_query_3");
  SetSeaPenFetcherResponse({249}, manta::MantaStatusCode::kOk, query);
  sea_pen_provider_remote()->GetSeaPenThumbnails(
      query->Clone(), search_wallpaper_future.GetCallback());
  ASSERT_EQ(249u, search_wallpaper_future.Get<0>().value().front()->id);
  search_wallpaper_future.Clear();

  // Selects from `search_query_2`.
  base::test::TestFuture<bool> select_wallpaper_future;
  sea_pen_provider_remote()->SelectSeaPenThumbnail(
      248, /*preview_mode=*/false, select_wallpaper_future.GetCallback());
  ASSERT_TRUE(select_wallpaper_future.Take());
  select_wallpaper_future.Clear();

  // Selects from `search_query_1`.
  sea_pen_provider_remote()->SelectSeaPenThumbnail(
      247, /*preview_mode=*/false, select_wallpaper_future.GetCallback());
  ASSERT_TRUE(select_wallpaper_future.Take());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, GetRecentSeaPenImageIds) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());

  // Create two images in the Sea Pen directory for the 1st user, then get the
  // list of the recent images.
  CreateSeaPenFilesForTesting(GetTestAccountId(), {kSeaPenId1, kSeaPenId2});

  base::test::TestFuture<const std::vector<uint32_t>&> recent_images_future;
  sea_pen_provider_remote()->GetRecentSeaPenImageIds(
      recent_images_future.GetCallback());

  std::vector<uint32_t> recent_images = recent_images_future.Take();
  EXPECT_THAT(recent_images,
              testing::UnorderedElementsAre(kSeaPenId1, kSeaPenId2));

  // Log in the second user, get the list of recent images.
  SetUpProfileForTesting(kFakeTestEmail2, GetTestAccountId2());

  sea_pen_provider_remote()->GetRecentSeaPenImageIds(
      recent_images_future.GetCallback());
  ASSERT_EQ(0u, recent_images_future.Take().size());

  // Create an image in the Sea Pen directory for second user, then get the list
  // of recent images again.
  CreateSeaPenFilesForTesting(GetTestAccountId2(), {kSeaPenId1});

  sea_pen_provider_remote()->GetRecentSeaPenImageIds(
      recent_images_future.GetCallback());
  recent_images = recent_images_future.Take();
  EXPECT_THAT(recent_images,
              testing::ContainerEq(std::vector<uint32_t>({kSeaPenId1})));
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       SelectThumbnailSendsFreeTextQuery) {
  auto time_override = CreateScopedTimeNowOverride();

  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());

  mojom::SeaPenQueryPtr search_query =
      mojom::SeaPenQuery::NewTextQuery("user search query text");

  // Send real images that will pass decoding.
  SetSeaPenFetcherResponse({111, 222}, manta::MantaStatusCode::kOk,
                           search_query);

  // Store some test images in the provider so that one can be selected.
  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;
  sea_pen_provider_remote()->GetSeaPenThumbnails(
      search_query.Clone(), search_wallpaper_future.GetCallback());
  // Select the first returned thumbnail.
  base::test::TestFuture<bool> select_wallpaper_future;
  sea_pen_provider_remote()->SelectSeaPenThumbnail(
      search_wallpaper_future.Get<0>().value().front()->id,
      /*preview_mode=*/false, select_wallpaper_future.GetCallback());

  ASSERT_TRUE(select_wallpaper_future.Take());

  // Verify the image was really saved with the correct metadata.
  base::test::TestFuture<const gfx::ImageSkia&,
                         personalization_app::mojom::RecentSeaPenImageInfoPtr>
      get_image_and_metadata_future;
  SeaPenWallpaperManager::GetInstance()->GetImageAndMetadata(
      GetTestAccountId(), 111, get_image_and_metadata_future.GetCallback());
  EXPECT_EQ(search_query->get_text_query(),
            get_image_and_metadata_future
                .Get<personalization_app::mojom::RecentSeaPenImageInfoPtr>()
                ->query->get_text_query());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       SelectThumbnailSendsTemplateQuery) {
  auto time_override = CreateScopedTimeNowOverride();

  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());

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

  // Send real images that will pass decoding.
  SetSeaPenFetcherResponse({111, 222}, manta::MantaStatusCode::kOk,
                           search_query);

  // Store some test images in the provider so that one can be selected.
  base::test::TestFuture<
      std::optional<
          std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>,
      manta::MantaStatusCode>
      search_wallpaper_future;

  sea_pen_provider_remote()->GetSeaPenThumbnails(
      search_query->Clone(), search_wallpaper_future.GetCallback());

  // Select the first returned thumbnail.
  base::test::TestFuture<bool> select_wallpaper_future;
  sea_pen_provider_remote()->SelectSeaPenThumbnail(
      search_wallpaper_future.Get<0>().value().front()->id,
      /*preview_mode=*/false, select_wallpaper_future.GetCallback());
  ASSERT_TRUE(select_wallpaper_future.Take());

  // Verify the image was really saved with the correct metadata.
  base::test::TestFuture<const gfx::ImageSkia&,
                         personalization_app::mojom::RecentSeaPenImageInfoPtr>
      get_image_and_metadata_future;
  SeaPenWallpaperManager::GetInstance()->GetImageAndMetadata(
      GetTestAccountId(), 111, get_image_and_metadata_future.GetCallback());
  EXPECT_TRUE(search_query->get_template_query().Equals(
      get_image_and_metadata_future
          .Get<personalization_app::mojom::RecentSeaPenImageInfoPtr>()
          ->query->get_template_query()));
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       GetRecentSeaPenImageThumbnailWithValidMetadata) {
  const auto time_override = CreateScopedTimeNowOverride();
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  const base::test::ScopedRestoreICUDefaultLocale locale("en_US");
  const base::test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  CreateSeaPenFilesForTesting(GetTestAccountId(), {kSeaPenId1});

  base::test::TestFuture<const std::vector<uint32_t>&> recent_images_future;
  sea_pen_provider_remote()->GetRecentSeaPenImageIds(
      recent_images_future.GetCallback());

  std::vector<uint32_t> recent_images = recent_images_future.Take();
  EXPECT_THAT(recent_images,
              testing::ContainerEq(std::vector<uint32_t>({kSeaPenId1})));

  base::test::TestFuture<mojom::RecentSeaPenThumbnailDataPtr>
      thumbnail_info_future;
  sea_pen_provider_remote()->GetRecentSeaPenImageThumbnail(
      recent_images[0], thumbnail_info_future.GetCallback());

  GURL url(thumbnail_info_future.Get()->url);
  EXPECT_FALSE(url.is_empty());
  EXPECT_EQ(base::TimeFormatShortDate(base::Time::Now()),
            thumbnail_info_future.Get()->image_info->creation_time.value());
  EXPECT_EQ("test query 111",
            thumbnail_info_future.Get()->image_info->query->get_text_query());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       GetRecentSeaPenImageThumbnailWithInvalidFilePath) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());

  CreateSeaPenFilesForTesting(GetTestAccountId(), {kSeaPenId1});

  base::test::TestFuture<const std::vector<uint32_t>&> recent_images_future;
  sea_pen_provider_remote()->GetRecentSeaPenImageIds(
      recent_images_future.GetCallback());

  std::vector<uint32_t> recent_images = recent_images_future.Take();
  EXPECT_THAT(recent_images,
              testing::ContainerEq(std::vector<uint32_t>({kSeaPenId1})));

  base::test::TestFuture<mojom::RecentSeaPenThumbnailDataPtr>
      thumbnail_info_future;
  // Try to get thumbnail data for an invalid Sea Pen id (not in the
  // `recent_images` list).
  sea_pen_provider_remote()->GetRecentSeaPenImageThumbnail(
      333, thumbnail_info_future.GetCallback());

  EXPECT_FALSE(thumbnail_info_future.Take());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       GetRecentSeaPenImageThumbnailWithDecodingFailure) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());

  CreateSeaPenFilesForTesting(GetTestAccountId(), {kSeaPenId1});
  {
    // Mess up the file so it fails decoding.
    const auto file_path = sea_pen_wallpaper_manager_session_delegate()
                               ->GetStorageDirectory(GetTestAccountId())
                               .Append(base::NumberToString(kSeaPenId1))
                               .AddExtension(".jpg");
    std::string data;
    ASSERT_TRUE(base::ReadFileToString(file_path, &data));
    // Cut off the last half of the data.
    data.erase(data.begin() + data.length() / 2);
    ASSERT_TRUE(base::WriteFile(file_path, data));
  }

  base::test::TestFuture<const std::vector<uint32_t>&> recent_images_future;
  sea_pen_provider_remote()->GetRecentSeaPenImageIds(
      recent_images_future.GetCallback());

  std::vector<uint32_t> recent_images = recent_images_future.Take();
  EXPECT_THAT(recent_images,
              testing::ContainerEq(std::vector<uint32_t>({kSeaPenId1})));

  base::test::TestFuture<mojom::RecentSeaPenThumbnailDataPtr>
      thumbnail_info_future;
  sea_pen_provider_remote()->GetRecentSeaPenImageThumbnail(
      recent_images[0], thumbnail_info_future.GetCallback());
  EXPECT_TRUE(thumbnail_info_future.Take().is_null());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, DeleteRecentSeaPenImage) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  test_wallpaper_controller()->ClearCounts();
  CreateSeaPenFilesForTesting(GetTestAccountId(), {kSeaPenId1, kSeaPenId2});

  base::test::TestFuture<const std::vector<uint32_t>&> recent_images_future;
  sea_pen_provider_remote()->GetRecentSeaPenImageIds(
      recent_images_future.GetCallback());
  EXPECT_THAT(recent_images_future.Take(),
              testing::UnorderedElementsAre(kSeaPenId1, kSeaPenId2));

  // Select the recent image |kSeaPenId1| as the current wallpaper.
  base::test::TestFuture<bool> select_wallpaper_future;
  sea_pen_provider_remote()->SelectRecentSeaPenImage(
      kSeaPenId1, /*preview_mode=*/false,
      select_wallpaper_future.GetCallback());
  EXPECT_TRUE(select_wallpaper_future.Take());

  // Delete |kSeaPenId2| from recent SeaPen images. |kSeaPenId1| is still the
  // current wallpaper.
  base::test::TestFuture<bool> delete_future;
  sea_pen_provider_remote()->DeleteRecentSeaPenImage(
      kSeaPenId2, delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Take());

  sea_pen_provider_remote()->GetRecentSeaPenImageIds(
      recent_images_future.GetCallback());
  EXPECT_THAT(recent_images_future.Take(),
              testing::UnorderedElementsAre(kSeaPenId1));
  EXPECT_EQ(WallpaperType::kSeaPen,
            test_wallpaper_controller()->wallpaper_info()->type);
  EXPECT_EQ(0, test_wallpaper_controller()->set_default_wallpaper_count());

  // Delete |kSeaPenId2| from recent SeaPen images. Should reset to default
  // wallpaper.
  sea_pen_provider_remote()->DeleteRecentSeaPenImage(
      kSeaPenId1, delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Take());

  sea_pen_provider_remote()->GetRecentSeaPenImageIds(
      recent_images_future.GetCallback());
  EXPECT_THAT(recent_images_future.Take(),
              testing::ContainerEq(std::vector<uint32_t>({})));
  EXPECT_EQ(1, test_wallpaper_controller()->set_default_wallpaper_count());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       ShouldShowSeaPenIntroductionDialog) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  test_wallpaper_controller()->ClearCounts();
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kSeaPen}, {});

  base::test::TestFuture<bool> should_show_dialog_future;
  sea_pen_provider_remote()->ShouldShowSeaPenIntroductionDialog(
      should_show_dialog_future.GetCallback());
  // Expects to return true before the dialog is closed.
  EXPECT_TRUE(should_show_dialog_future.Take());

  sea_pen_provider_remote()->HandleSeaPenIntroductionDialogClosed();

  sea_pen_provider_remote()->ShouldShowSeaPenIntroductionDialog(
      should_show_dialog_future.GetCallback());
  // Expects to return false after the dialog is closed.
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

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       IsManagedSeaPenFeedbackEnabledGoogler) {
  SetUpProfileForTesting(kGooglerEmail, GetGooglerAccountId());
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  profile()->GetPrefs()->SetInteger(
      ash::prefs::kGenAIWallpaperSettings,
      static_cast<int>(ManagedSeaPenSettings::kAllowedWithoutLogging));
  ASSERT_TRUE(sea_pen_provider()->IsManagedSeaPenFeedbackEnabled())
      << " SeaPen Wallpaper feedback should be enabled for Googlers";
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       IsManagedSeaPenFeedbackEnabledPublicAccountDemoMode) {
  SetUpProfileForTesting(kDemoModeEmail, GetDemoModeAccountId(),
                         user_manager::UserType::kPublicAccount);
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  profile()->GetPrefs()->SetInteger(
      ash::prefs::kGenAIWallpaperSettings,
      static_cast<int>(ManagedSeaPenSettings::kAllowedWithoutLogging));

  // Force device into demo mode.
  ASSERT_FALSE(::ash::DemoSession::IsDeviceInDemoMode());
  profile()->ScopedCrosSettingsTestHelper()->InstallAttributes()->SetDemoMode();
  ASSERT_TRUE(::ash::DemoSession::IsDeviceInDemoMode());

  // Force demo mode session to start.
  ASSERT_FALSE(::ash::DemoSession::Get());
  auto demo_mode_test_helper = std::make_unique<::ash::DemoModeTestHelper>();
  demo_mode_test_helper->InitializeSession();
  ASSERT_TRUE(::ash::DemoSession::Get());

  ASSERT_TRUE(sea_pen_provider()->IsManagedSeaPenFeedbackEnabled())
      << " SeaPen Wallpaper feedback should be enabled for Demo Mode";
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       IsManagedSeaPenFeedbackEnabledRegular) {
  SetUpProfileForTesting(kFakeTestEmail2, GetTestAccountId2());
  ASSERT_TRUE(sea_pen_provider()->IsManagedSeaPenFeedbackEnabled());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       IsManagedSeaPenFeedbackEnabledAllowedManaged) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  profile()->GetPrefs()->SetInteger(
      ash::prefs::kGenAIWallpaperSettings,
      static_cast<int>(ManagedSeaPenSettings::kAllowed));
  ASSERT_TRUE(sea_pen_provider()->IsManagedSeaPenFeedbackEnabled());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       IsManagedSeaPenFeedbackEnabledAllowedWithoutLoggingManaged) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  profile()->GetPrefs()->SetInteger(
      ash::prefs::kGenAIWallpaperSettings,
      static_cast<int>(ManagedSeaPenSettings::kAllowedWithoutLogging));
  ASSERT_FALSE(sea_pen_provider()->IsManagedSeaPenFeedbackEnabled());
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       IsManagedSeaPenFeedbackEnabledDisabledManaged) {
  SetUpProfileForTesting(kFakeTestEmail, GetTestAccountId());
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  profile()->GetPrefs()->SetInteger(
      ash::prefs::kGenAIWallpaperSettings,
      static_cast<int>(ManagedSeaPenSettings::kDisabled));
  ASSERT_FALSE(sea_pen_provider()->IsManagedSeaPenFeedbackEnabled());
}
}  // namespace

}  // namespace ash::personalization_app
