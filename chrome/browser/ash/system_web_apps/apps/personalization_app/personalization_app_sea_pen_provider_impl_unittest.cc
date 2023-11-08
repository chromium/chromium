// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_impl.h"

#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_image_decoder.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/manta/features.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash::personalization_app {

namespace {

using std::literals::string_view_literals::operator""sv;

constexpr char kFakeTestEmail[] = "fakeemail@personalization";
constexpr char kTestGaiaId[] = "1234567890";

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

void AddAndLoginUser(const AccountId& account_id) {
  ash::FakeChromeUserManager* user_manager =
      static_cast<ash::FakeChromeUserManager*>(
          user_manager::UserManager::Get());

  user_manager->AddUser(account_id);
  user_manager->LoginUser(account_id);
  user_manager->SwitchActiveUser(account_id);
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

std::vector<data_decoder::mojom::AnimationFramePtr>
TestDecodeAnimationCallback() {
  return {};
}

SkBitmap TestDecodeImageCallback() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(4, 4);
  bitmap.eraseColor(SK_ColorMAGENTA);
  return bitmap;
}

class PersonalizationAppSeaPenProviderImplTest : public testing::Test {
 public:
  PersonalizationAppSeaPenProviderImplTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures(
        {manta::features::kMantaService, features::kSeaPen}, {});
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

    test_image_decoder_ = std::make_unique<::ash::TestImageDecoder>(
        base::BindRepeating(&TestDecodeAnimationCallback),
        base::BindRepeating(&TestDecodeImageCallback));

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);

    AddAndLoginUser(GetTestAccountId());

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    sea_pen_provider_ = std::make_unique<PersonalizationAppSeaPenProviderImpl>(
        &web_ui_,
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<::ash::TestImageDecoder> test_image_decoder_;
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

TEST_F(PersonalizationAppSeaPenProviderImplTest, SearchReturnsThumbnails) {
  base::test::TestFuture<absl::optional<
      std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>>
      search_wallpaper_future;
  sea_pen_provider_remote()->SearchWallpaper(
      "search_query", search_wallpaper_future.GetCallback());
  EXPECT_THAT(
      search_wallpaper_future.Take().value(),
      testing::ElementsAre(MatchesSeaPenImage("fake_sea_pen_image_1"sv, 1),
                           MatchesSeaPenImage("fake_sea_pen_image_2"sv, 2),
                           MatchesSeaPenImage("fake_sea_pen_image_3"sv, 3),
                           MatchesSeaPenImage("fake_sea_pen_image_4"sv, 4)));
}

TEST_F(PersonalizationAppSeaPenProviderImplTest, MaxLengthQuery) {
  // "\uFFFF" is picked because `.size()` differs by a factor of three
  // between UTF-8 (C++ std::string) and UTF-16 (javascript string).
  std::string long_unicode_string =
      RepeatToSize("\uFFFF", mojom::kMaximumSearchWallpaperTextBytes);
  ASSERT_EQ(mojom::kMaximumSearchWallpaperTextBytes,
            long_unicode_string.size());
  // In javascript UTF-16, `long_unicode_string.length` is 1/3.
  ASSERT_EQ(mojom::kMaximumSearchWallpaperTextBytes / 3,
            base::UTF8ToUTF16(long_unicode_string).size());

  base::test::TestFuture<absl::optional<
      std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>>
      search_wallpaper_future;
  sea_pen_provider_remote()->SearchWallpaper(
      long_unicode_string, search_wallpaper_future.GetCallback());
  EXPECT_EQ(4u, search_wallpaper_future.Take().value().size())
      << "SearchWallpaper succeeds if text is exactly max length";

  mojo::test::BadMessageObserver bad_message_observer;
  sea_pen_provider_remote()->SearchWallpaper(
      long_unicode_string + "a",
      base::BindLambdaForTesting(
          [](absl::optional<std::vector<
                 ash::personalization_app::mojom::SeaPenThumbnailPtr>>) {
            NOTREACHED();
          }));
  EXPECT_EQ("SearchWallpaper exceeded maximum text length",
            bad_message_observer.WaitForBadMessage())
      << "SearchWallpaper fails if text is longer than max length";
}

TEST_F(PersonalizationAppSeaPenProviderImplTest,
       SelectThumbnailSetsOneShotWallpaper) {
  // Store some test images in the provider so that one can be selected.
  base::test::TestFuture<absl::optional<
      std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr>>>
      search_wallpaper_future;
  sea_pen_provider_remote()->SearchWallpaper(
      "search_query", search_wallpaper_future.GetCallback());

  ASSERT_EQ(0, test_wallpaper_controller()->get_one_shot_wallpaper_count());

  // Select the first returned thumbnail.
  base::test::TestFuture<bool> select_wallpaper_future;
  sea_pen_provider_remote()->SelectSeaPenThumbnail(
      search_wallpaper_future.Take()->front()->id,
      select_wallpaper_future.GetCallback());

  ASSERT_TRUE(select_wallpaper_future.Take());
  EXPECT_EQ(1, test_wallpaper_controller()->get_one_shot_wallpaper_count());
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      TestDecodeImageCallback(),
      *test_wallpaper_controller()->GetWallpaperImage().bitmap()));
}

}  // namespace

}  // namespace ash::personalization_app
