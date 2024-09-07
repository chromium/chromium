// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_wallpaper_provider_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "ash/wallpaper/test_sea_pen_wallpaper_manager_session_delegate.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_pref_manager.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "ash/webui/common/mojom/sea_pen_generated.mojom-shared.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_wallpaper_image_external_data_handler.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/mock_personalization_app_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager_factory.h"
#include "chrome/browser/ash/wallpaper_handlers/mock_wallpaper_handlers.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/ash/wallpaper/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace ash::personalization_app {

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@personalization";
constexpr char kTestGaiaId[] = "1234567890";

// Create fake Jpg image bytes.
std::string CreateJpgBytes() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseARGB(255, 31, 63, 127);
  std::vector<unsigned char> data;
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/100, &data);
  return std::string(data.begin(), data.end());
}

TestingPrefServiceSimple* RegisterPrefs(TestingPrefServiceSimple* local_state) {
  ash::device_settings_cache::RegisterPrefs(local_state->registry());
  user_manager::KnownUser::RegisterPrefs(local_state->registry());
  ash::WallpaperPrefManager::RegisterLocalStatePrefs(local_state->registry());
  policy::DeviceWallpaperImageExternalDataHandler::RegisterPrefs(
      local_state->registry());
  ProfileAttributesStorage::RegisterPrefs(local_state->registry());
  return local_state;
}

void AddAndLoginUser(const AccountId& account_id) {
  ash::FakeChromeUserManager* user_manager =
      static_cast<ash::FakeChromeUserManager*>(
          user_manager::UserManager::Get());
  user_manager->AddUser(account_id);
  user_manager->LoginUser(account_id);
  user_manager->SwitchActiveUser(account_id);
}

AccountId GetTestAccountId() {
  return AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId);
}

// Create a test 1x1 image with a given |color|.
gfx::ImageSkia CreateSolidImageSkia(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

std::unique_ptr<KeyedService> MakeMockPersonalizationAppManager(
    content::BrowserContext* context) {
  return std::make_unique<::testing::NiceMock<MockPersonalizationAppManager>>();
}

class TestWallpaperObserver
    : public ash::personalization_app::mojom::WallpaperObserver {
 public:
  void WaitForAttributionChange() {
    ASSERT_FALSE(on_attribution_changed_callback_);
    base::RunLoop loop;
    on_attribution_changed_callback_ = loop.QuitClosure();
    loop.Run();
  }

  // WallpaperObserver:
  void OnWallpaperPreviewEnded() override {}

  void OnWallpaperChanged(
      ash::personalization_app::mojom::CurrentWallpaperPtr image) override {
    current_wallpaper_ = std::move(image);
  }

  void OnAttributionChanged(
      ash::personalization_app::mojom::CurrentAttributionPtr attribution)
      override {
    current_attribution_ = std::move(attribution);
    if (on_attribution_changed_callback_) {
      std::move(on_attribution_changed_callback_).Run();
    }
  }

  mojo::PendingRemote<ash::personalization_app::mojom::WallpaperObserver>
  pending_remote() {
    DCHECK(!wallpaper_observer_receiver_.is_bound());
    return wallpaper_observer_receiver_.BindNewPipeAndPassRemote();
  }

  ash::personalization_app::mojom::CurrentWallpaper* current_wallpaper() {
    if (!wallpaper_observer_receiver_.is_bound()) {
      return nullptr;
    }

    wallpaper_observer_receiver_.FlushForTesting();
    return current_wallpaper_.get();
  }

  ash::personalization_app::mojom::CurrentAttribution* current_attribution() {
    if (!wallpaper_observer_receiver_.is_bound()) {
      return nullptr;
    }

    wallpaper_observer_receiver_.FlushForTesting();
    return current_attribution_.get();
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::WallpaperObserver>
      wallpaper_observer_receiver_{this};

  ash::personalization_app::mojom::CurrentWallpaperPtr current_wallpaper_ =
      nullptr;

  ash::personalization_app::mojom::CurrentAttributionPtr current_attribution_ =
      nullptr;

  base::OnceClosure on_attribution_changed_callback_;
};

}  // namespace

class PersonalizationAppWallpaperProviderImplTest : public testing::Test {
 public:
  PersonalizationAppWallpaperProviderImplTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  PersonalizationAppWallpaperProviderImplTest(
      const PersonalizationAppWallpaperProviderImplTest&) = delete;
  PersonalizationAppWallpaperProviderImplTest& operator=(
      const PersonalizationAppWallpaperProviderImplTest&) = delete;
  ~PersonalizationAppWallpaperProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    sea_pen_wallpaper_manager()->SetSessionDelegateForTesting(
        std::make_unique<TestSeaPenWallpaperManagerSessionDelegate>());

    wallpaper_controller_client_ = std::make_unique<
        WallpaperControllerClientImpl>(
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(
        kFakeTestEmail,
        {TestingProfile::TestingFactory{
            ash::personalization_app::PersonalizationAppManagerFactory::
                GetInstance(),
            base::BindRepeating(&MakeMockPersonalizationAppManager)}});

    AddAndLoginUser(GetTestAccountId());
    test_wallpaper_controller()->SetCurrentUser(GetTestAccountId());

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    wallpaper_provider_ = std::make_unique<
        PersonalizationAppWallpaperProviderImpl>(
        &web_ui_,
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());

    wallpaper_provider_->BindInterface(
        wallpaper_provider_remote_.BindNewPipeAndPassReceiver());
  }

  PersonalizationAppWallpaperProviderImpl::ImageInfo GetDefaultImageInfo() {
    return {
        /*in_image_url=*/GURL("http://test_url"),
        /*in_collection_id=*/"collection_id",
        /*in_asset_id=*/1,
        /*in_unit_id=*/1,
        /*in_type=*/backdrop::Image::IMAGE_TYPE_UNKNOWN,
    };
  }

  void AddWallpaperImage(
      const PersonalizationAppWallpaperProviderImpl::ImageInfo& image_info) {
    wallpaper_provider_->image_unit_id_map_.insert(
        {image_info.unit_id, {image_info}});
  }

  SeaPenWallpaperManager* sea_pen_wallpaper_manager() {
    return &sea_pen_wallpaper_manager_;
  }

  TestSeaPenWallpaperManagerSessionDelegate*
  sea_pen_wallpaper_manager_session_delegate() {
    return static_cast<TestSeaPenWallpaperManagerSessionDelegate*>(
        sea_pen_wallpaper_manager()->session_delegate_for_testing());
  }

  TestWallpaperController* test_wallpaper_controller() {
    return &test_wallpaper_controller_;
  }

  TestingProfile* profile() { return profile_; }

  mojo::Remote<ash::personalization_app::mojom::WallpaperProvider>&
  wallpaper_provider_remote() {
    return wallpaper_provider_remote_;
  }

  PersonalizationAppWallpaperProviderImpl* delegate() {
    return wallpaper_provider_.get();
  }

  void ResetWallpaperProvider() { wallpaper_provider_.reset(); }

  ::testing::NiceMock<MockPersonalizationAppManager>*
  MockPersonalizationAppManager() {
    return static_cast<::testing::NiceMock<
        ::ash::personalization_app::MockPersonalizationAppManager>*>(
        ::ash::personalization_app::PersonalizationAppManagerFactory::
            GetForBrowserContext(profile_));
  }

  void SetWallpaperObserver() {
    wallpaper_provider_remote_->SetWallpaperObserver(
        test_wallpaper_observer_.pending_remote());
  }

  TestWallpaperObserver* test_wallpaper_observer() {
    return &test_wallpaper_observer_;
  }

  ash::personalization_app::mojom::CurrentWallpaper* current_wallpaper() {
    wallpaper_provider_remote_.FlushForTesting();
    return test_wallpaper_observer_.current_wallpaper();
  }

  ash::personalization_app::mojom::CurrentAttribution* current_attribution() {
    wallpaper_provider_remote_.FlushForTesting();
    return test_wallpaper_observer_.current_attribution();
  }

 private:
  // Note: `scoped_feature_list_` should be destroyed after `task_environment_`
  // (see crbug.com/846380).
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  InProcessDataDecoder in_process_data_decoder_;
  TestingPrefServiceSimple pref_service_;
  // Required for CrosSettings.
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  // Required for CrosSettings.
  ash::ScopedTestDeviceSettingsService scoped_device_settings_;
  // Required for |WallpaperControllerClientImpl|.
  ash::CrosSettingsHolder cros_settings_holder_{
      ash::DeviceSettingsService::Get(), RegisterPrefs(&pref_service_)};
  user_manager::ScopedUserManager scoped_user_manager_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  SeaPenWallpaperManager sea_pen_wallpaper_manager_;
  TestWallpaperController test_wallpaper_controller_;
  // |wallpaper_controller_client_| must be destructed before
  // |test_wallpaper_controller_|.
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  mojo::Remote<ash::personalization_app::mojom::WallpaperProvider>
      wallpaper_provider_remote_;
  TestWallpaperObserver test_wallpaper_observer_;
  std::unique_ptr<PersonalizationAppWallpaperProviderImpl> wallpaper_provider_;
};

TEST_F(PersonalizationAppWallpaperProviderImplTest, SelectWallpaper) {
  test_wallpaper_controller()->ClearCounts();

  auto image_info = GetDefaultImageInfo();
  std::vector<ash::OnlineWallpaperVariant> variants;
  variants.emplace_back(image_info.asset_id, image_info.image_url,
                        backdrop::Image::IMAGE_TYPE_UNKNOWN);

  AddWallpaperImage(image_info);

  base::test::TestFuture<bool> success_future;
  wallpaper_provider_remote()->SelectWallpaper(image_info.asset_id,
                                               /*preview_mode=*/false,
                                               success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  EXPECT_EQ(1, test_wallpaper_controller()->set_online_wallpaper_count());
  EXPECT_TRUE(
      test_wallpaper_controller()->wallpaper_info().value().MatchesSelection(
          ash::WallpaperInfo(
              {GetTestAccountId(), "collection_id",
               ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
               /*preview_mode=*/false, /*from_user=*/true,
               /*daily_refresh_enabled=*/false, image_info.unit_id, variants},
              variants.front())));
}

TEST_F(PersonalizationAppWallpaperProviderImplTest, SelectWallpaperWhenBanned) {
  test_wallpaper_controller()->set_can_set_user_wallpaper(false);
  auto image_info = GetDefaultImageInfo();
  std::vector<ash::OnlineWallpaperVariant> variants;
  variants.emplace_back(image_info.asset_id, image_info.image_url,
                        backdrop::Image::IMAGE_TYPE_UNKNOWN);

  AddWallpaperImage(image_info);

  mojo::test::BadMessageObserver bad_message_observer;

  wallpaper_provider_remote()->SelectWallpaper(
      image_info.asset_id, /*preview_mode=*/false,
      base::BindLambdaForTesting(
          [](bool success) { NOTREACHED_IN_MIGRATION(); }));

  EXPECT_EQ("Invalid request to set wallpaper",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(PersonalizationAppWallpaperProviderImplTest, PreviewWallpaper) {
  test_wallpaper_controller()->ClearCounts();

  auto image_info = GetDefaultImageInfo();
  std::vector<ash::OnlineWallpaperVariant> variants;
  variants.emplace_back(image_info.asset_id, image_info.image_url,
                        image_info.type);

  AddWallpaperImage(image_info);

  base::test::TestFuture<bool> success_future;
  wallpaper_provider_remote()->SelectWallpaper(image_info.asset_id,
                                               /*preview_mode=*/true,
                                               success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  EXPECT_EQ(1, test_wallpaper_controller()->set_online_wallpaper_count());
  EXPECT_TRUE(
      test_wallpaper_controller()->wallpaper_info().value().MatchesSelection(
          ash::WallpaperInfo(
              {GetTestAccountId(), "collection_id",
               ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
               /*preview_mode=*/true, /*from_user=*/true,
               /*daily_refresh_enabled=*/false, image_info.unit_id, variants},
              variants.front())));
}

TEST_F(PersonalizationAppWallpaperProviderImplTest,
       ObserveWallpaperFiresWhenBound) {
  test_wallpaper_controller()->ShowWallpaperImage(
      CreateSolidImageSkia(/*width=*/1, /*height=*/1, SK_ColorBLACK));

  auto image_info = GetDefaultImageInfo();
  std::vector<ash::OnlineWallpaperVariant> variants;
  variants.emplace_back(image_info.asset_id, image_info.image_url,
                        backdrop::Image::IMAGE_TYPE_UNKNOWN);

  AddWallpaperImage(image_info);

  test_wallpaper_controller()->SetOnlineWallpaper(
      {GetTestAccountId(), "collection_id",
       ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*from_user=*/true,
       /*daily_refresh_enabled=*/false, image_info.unit_id, variants},
      base::DoNothing());

  EXPECT_EQ(nullptr, current_wallpaper());

  SetWallpaperObserver();

  // WallpaperObserver Should have received an image through mojom.
  ash::personalization_app::mojom::CurrentWallpaper* current =
      current_wallpaper();

  EXPECT_EQ(ash::WallpaperType::kOnline, current->type);
  EXPECT_EQ(ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
            current->layout);
}

TEST_F(PersonalizationAppWallpaperProviderImplTest,
       IgnoresWallpaperResizeForOtherUser) {
  const AccountId other_account_id = AccountId::FromUserEmailGaiaId(
      "otherfakeemail@personalization", "0987654321");
  test_wallpaper_controller()->SetCurrentUser(other_account_id);

  test_wallpaper_controller()->ShowWallpaperImage(
      CreateSolidImageSkia(/*width=*/1, /*height=*/1, SK_ColorBLACK));

  auto image_info = GetDefaultImageInfo();
  std::vector<ash::OnlineWallpaperVariant> variants;
  variants.emplace_back(image_info.asset_id, image_info.image_url,
                        backdrop::Image::IMAGE_TYPE_UNKNOWN);

  AddWallpaperImage(image_info);

  test_wallpaper_controller()->SetOnlineWallpaper(
      {other_account_id, "collection_id",
       ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*from_user=*/true,
       /*daily_refresh_enabled=*/false, image_info.unit_id, variants},
      base::DoNothing());

  EXPECT_EQ(nullptr, current_wallpaper());

  SetWallpaperObserver();

  EXPECT_EQ(nullptr, current_wallpaper());
}

TEST_F(PersonalizationAppWallpaperProviderImplTest, ValidSeaPenAttribution) {
  {
    // Save the image and metadata to disk.
    mojom::SeaPenQueryPtr sea_pen_query_ptr =
        mojom::SeaPenQuery::NewTemplateQuery(mojom::SeaPenTemplateQuery::New(
            mojom::SeaPenTemplateId::kArt,
            base::flat_map<mojom::SeaPenTemplateChip,
                           mojom::SeaPenTemplateOption>(
                {{mojom::SeaPenTemplateChip::kArtFeature,
                  mojom::SeaPenTemplateOption::kArtFeatureBeach},
                 {mojom::SeaPenTemplateChip::kArtMovement,
                  mojom::SeaPenTemplateOption::kArtMovementAbstract}}),
            mojom::SeaPenUserVisibleQuery::New("test template query text",
                                               "test template query title")));

    base::test::TestFuture<bool> save_sea_pen_image_future;
    sea_pen_wallpaper_manager()->SaveSeaPenImage(
        GetTestAccountId(), {CreateJpgBytes(), 111u}, sea_pen_query_ptr,
        save_sea_pen_image_future.GetCallback());
    ASSERT_TRUE(save_sea_pen_image_future.Get());
  }

  // Set the image as user wallpaper.
  test_wallpaper_controller()->SetSeaPenWallpaper(
      GetTestAccountId(), 111u, /*preview_mode=*/false, base::DoNothing());

  SetWallpaperObserver();
  test_wallpaper_observer()->WaitForAttributionChange();

  ash::personalization_app::mojom::CurrentAttribution* current_attr =
      current_attribution();
  EXPECT_EQ("111", current_attr->key);
  std::vector<std::string> expected_attr{
      "test template query text",
      l10n_util::GetStringUTF8(IDS_SEA_PEN_POWERED_BY_GOOGLE_AI)};
  EXPECT_EQ(expected_attr, current_attr->attribution);
}

TEST_F(PersonalizationAppWallpaperProviderImplTest, MissingSeaPenAttribution) {
  // Write a jpg with no metadata.
  const base::FilePath jpg_path = sea_pen_wallpaper_manager_session_delegate()
                                      ->GetStorageDirectory(GetTestAccountId())
                                      .Append("111")
                                      .AddExtension(".jpg");
  ASSERT_TRUE(base::CreateDirectory(jpg_path.DirName()));
  ASSERT_TRUE(base::WriteFile(jpg_path, CreateJpgBytes()));

  test_wallpaper_controller()->SetSeaPenWallpaper(
      GetTestAccountId(), 111u, /*preview_mode=*/false, base::DoNothing());

  SetWallpaperObserver();
  test_wallpaper_observer()->WaitForAttributionChange();

  ash::personalization_app::mojom::CurrentAttribution* current_attr =
      current_attribution();
  EXPECT_EQ("111", current_attr->key);
  EXPECT_EQ(std::vector<std::string>(), current_attr->attribution);
}

TEST_F(PersonalizationAppWallpaperProviderImplTest, SetCurrentWallpaperLayout) {
  auto* ctrl = test_wallpaper_controller();

  EXPECT_EQ(ctrl->update_current_wallpaper_layout_count(), 0);
  EXPECT_EQ(ctrl->update_current_wallpaper_layout_layout(), std::nullopt);

  auto layout = ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER;
  wallpaper_provider_remote()->SetCurrentWallpaperLayout(layout);
  wallpaper_provider_remote().FlushForTesting();

  EXPECT_EQ(ctrl->update_current_wallpaper_layout_count(), 1);
  EXPECT_EQ(ctrl->update_current_wallpaper_layout_layout(), layout);
}

TEST_F(PersonalizationAppWallpaperProviderImplTest,
       CallsMaybeStartHatsTimerOnDestruction) {
  // Insert a wallpaper image info to indicate that the user opened the
  // wallpaper app and requested wallpapers.
  AddWallpaperImage(GetDefaultImageInfo());

  EXPECT_CALL(*MockPersonalizationAppManager(),
              MaybeStartHatsTimer(HatsSurveyType::kWallpaper))
      .Times(1);

  ResetWallpaperProvider();
}

TEST_F(PersonalizationAppWallpaperProviderImplTest,
       DoesNotCallMaybeStartHatsTimerIfNoWallpaperFetched) {
  EXPECT_CALL(*MockPersonalizationAppManager(),
              MaybeStartHatsTimer(HatsSurveyType::kWallpaper))
      .Times(0);

  ResetWallpaperProvider();
}

TEST_F(PersonalizationAppWallpaperProviderImplTest, GetWallpaperAsJpegBytes) {
  auto image = CreateSolidImageSkia(/*width=*/1, /*height=*/1, SK_ColorRED);
  test_wallpaper_controller()->ShowWallpaperImage(image);
  scoped_refptr<base::RefCountedMemory> expected_jpeg_bytes =
      gfx::Image(image).As1xPNGBytes();

  // Jpeg bytes of the preview image.
  base::RunLoop loop;
  test_wallpaper_controller()->LoadPreviewImage(base::BindLambdaForTesting(
      [quit = loop.QuitClosure(), &expected_jpeg_bytes](
          scoped_refptr<base::RefCountedMemory> preview_bytes) {
        EXPECT_TRUE(preview_bytes->Equals(expected_jpeg_bytes));
        std::move(quit).Run();
      }));
  loop.Run();
}

TEST_F(PersonalizationAppWallpaperProviderImplTest, SetDailyRefreshBanned) {
  EXPECT_EQ(test_wallpaper_controller()->GetDailyRefreshCollectionId(
                GetTestAccountId()),
            "");

  const std::string collection_id = "collection_id";
  test_wallpaper_controller()->set_can_set_user_wallpaper(false);
  mojo::test::BadMessageObserver bad_message_observer;
  wallpaper_provider_remote()->SetDailyRefreshCollectionId(
      collection_id, base::BindLambdaForTesting(
                         [](bool success) { NOTREACHED_IN_MIGRATION(); }));
  EXPECT_EQ("Invalid request to set wallpaper",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(PersonalizationAppWallpaperProviderImplTest,
       ShouldShowTimeOfDayWallpaperDialog) {
  test_wallpaper_controller()->ClearCounts();
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kFeatureManagementTimeOfDayWallpaper},
                            {});

  auto image_info = GetDefaultImageInfo();
  image_info.collection_id =
      wallpaper_constants::kTimeOfDayWallpaperCollectionId;
  std::vector<ash::OnlineWallpaperVariant> variants;
  variants.emplace_back(image_info.asset_id, image_info.image_url,
                        backdrop::Image::IMAGE_TYPE_UNKNOWN);

  AddWallpaperImage(image_info);

  base::test::TestFuture<bool> should_show_dialog_future;
  wallpaper_provider_remote()->ShouldShowTimeOfDayWallpaperDialog(
      should_show_dialog_future.GetCallback());
  // Expects to return true before time of day wallpaper is set.
  EXPECT_TRUE(should_show_dialog_future.Take());

  base::test::TestFuture<bool> success_future;
  wallpaper_provider_remote()->SelectWallpaper(image_info.asset_id,
                                               /*preview_mode=*/false,
                                               success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  EXPECT_EQ(1, test_wallpaper_controller()->set_online_wallpaper_count());
  EXPECT_TRUE(
      test_wallpaper_controller()->wallpaper_info().value().MatchesSelection(
          ash::WallpaperInfo(
              {GetTestAccountId(),
               wallpaper_constants::kTimeOfDayWallpaperCollectionId,
               ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
               /*preview_mode=*/false, /*from_user=*/true,
               /*daily_refresh_enabled=*/false, image_info.unit_id, variants},
              variants.front())));

  wallpaper_provider_remote()->ShouldShowTimeOfDayWallpaperDialog(
      should_show_dialog_future.GetCallback());
  // Expects to return false after time of day wallpaper is set.
  EXPECT_FALSE(should_show_dialog_future.Take());
}

class PersonalizationAppWallpaperProviderImplGooglePhotosTest
    : public PersonalizationAppWallpaperProviderImplTest {
 protected:
  // Mocks an attempt to fetch the Google Photos enterprise setting from the
  // server. A successful attempt, which happens when the Google Photos
  // wallpaper integration is enabled, will enable wallpaper selection and
  // other Google Photos data fetches to go through.
  void FetchGooglePhotosEnabled(size_t num_fetches = 1) {
    using ash::personalization_app::mojom::GooglePhotosEnablementState;

    // Mock a fetcher for the enablement state query.
    auto* const google_photos_enabled_fetcher = static_cast<::testing::NiceMock<
        wallpaper_handlers::MockGooglePhotosEnabledFetcher>*>(
        delegate()->GetOrCreateGooglePhotosEnabledFetcher());

    EXPECT_CALL(*google_photos_enabled_fetcher, AddRequestAndStartIfNecessary)
        .Times(num_fetches);

    for (size_t i = 0; i < num_fetches; ++i) {
      base::test::TestFuture<GooglePhotosEnablementState> future;
      wallpaper_provider_remote()->FetchGooglePhotosEnabled(
          future.GetCallback());
      EXPECT_EQ(GooglePhotosEnablementState::kEnabled, future.Take());
    }
  }

  void AddToAlbumIdMap(const std::string& album_id,
                       const std::string& dedup_key) {
    std::set<std::string> dedup_keys;
    dedup_keys.insert(dedup_key);
    delegate()->album_id_dedup_key_map_.insert({album_id, dedup_keys});
  }

  // The number of times to start each idempotent API query.
  static constexpr size_t kNumFetches = 2;
  // Resume token value used across several tests.
  const std::string kResumeToken = "token";
};

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest, FetchAlbums) {
  // Mock a fetcher for the albums query.
  auto* const google_photos_albums_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosAlbumsFetcher>*>(
      delegate()->GetOrCreateGooglePhotosAlbumsFetcher());

  auto* const google_photos_shared_albums_fetcher =
      static_cast<::testing::NiceMock<
          wallpaper_handlers::MockGooglePhotosSharedAlbumsFetcher>*>(
          delegate()->GetOrCreateGooglePhotosSharedAlbumsFetcher());

  // Simulate the client making multiple requests for the same information to
  // test that all callbacks for that query are called.
  EXPECT_CALL(*google_photos_albums_fetcher,
              AddRequestAndStartIfNecessary(std::make_optional(kResumeToken),
                                            ::testing::_))
      .Times(kNumFetches);

  EXPECT_CALL(*google_photos_shared_albums_fetcher,
              AddRequestAndStartIfNecessary(std::make_optional(kResumeToken),
                                            ::testing::_))
      .Times(kNumFetches);

  // Test fetching Google Photos albums after fetching the enterprise setting.
  // Requests should be made if and only if the Google Photos wallpaper
  // integration is enabled.
  FetchGooglePhotosEnabled();
  for (size_t i = 0; i < kNumFetches; ++i) {
    base::test::TestFuture<
        ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponsePtr>
        albums_response;
    wallpaper_provider_remote()->FetchGooglePhotosAlbums(
        kResumeToken, albums_response.GetCallback());
    EXPECT_TRUE(albums_response.Take()->albums.has_value());

    base::test::TestFuture<
        ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponsePtr>
        shared_albums_response;
    wallpaper_provider_remote()->FetchGooglePhotosSharedAlbums(
        kResumeToken, shared_albums_response.GetCallback());
    EXPECT_TRUE(shared_albums_response.Take()->albums.has_value());
  }
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       FetchAlbumsBeforeEnabled) {
  // Mock a fetcher for the albums query.
  auto* const google_photos_albums_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosAlbumsFetcher>*>(
      delegate()->GetOrCreateGooglePhotosAlbumsFetcher());

  // The albums fetcher should never be called.
  EXPECT_CALL(*google_photos_albums_fetcher, AddRequestAndStartIfNecessary)
      .Times(0);

  mojo::test::BadMessageObserver bad_message_observer;
  // Test fetching Google Photos albums before fetching the enterprise enabled
  // setting. No requests should be made.
  wallpaper_provider_remote()->FetchGooglePhotosAlbums(
      kResumeToken, base::BindLambdaForTesting(
                        [](mojom::FetchGooglePhotosAlbumsResponsePtr response) {
                          NOTREACHED_IN_MIGRATION();
                        }));
  EXPECT_EQ(
      "Cannot call `FetchGooglePhotosAlbums()` without confirming that the "
      "Google Photos enterprise setting is enabled.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest, FetchEnabled) {
  // Simulate the client making multiple requests for the same information to
  // test that all callbacks for that query are called.
  FetchGooglePhotosEnabled(kNumFetches);
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest, FetchPhotos) {
  // Mock a fetcher for the photos query.
  auto* const google_photos_photos_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosPhotosFetcher>*>(
      delegate()->GetOrCreateGooglePhotosPhotosFetcher());

  // Simulate the client making multiple requests for the same information to
  // test that all callbacks for that query are called.
  const std::string item_id = "itemId";
  const std::string album_id = "albumId";
  EXPECT_CALL(*google_photos_photos_fetcher,
              AddRequestAndStartIfNecessary(
                  std::make_optional(item_id), std::make_optional(album_id),
                  std::make_optional(kResumeToken), false, ::testing::_))
      .Times(kNumFetches);

  // Test fetching Google Photos photos after fetching the enterprise setting.
  // Requests should be made if and only if the Google Photos wallpaper
  // integration is enabled.
  FetchGooglePhotosEnabled();
  for (size_t i = 0; i < kNumFetches; ++i) {
    base::test::TestFuture<
        ash::personalization_app::mojom::FetchGooglePhotosPhotosResponsePtr>
        photos_response;
    wallpaper_provider_remote()->FetchGooglePhotosPhotos(
        item_id, album_id, kResumeToken, photos_response.GetCallback());
    EXPECT_TRUE(photos_response.Take()->photos.has_value());
  }
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       FetchPhotosBeforeEnabled) {
  auto* const google_photos_photos_fetcher = static_cast<
      ::testing::NiceMock<wallpaper_handlers::MockGooglePhotosPhotosFetcher>*>(
      delegate()->GetOrCreateGooglePhotosPhotosFetcher());

  const std::string item_id = "itemId";
  const std::string album_id = "albumId";

  EXPECT_CALL(*google_photos_photos_fetcher, AddRequestAndStartIfNecessary)
      .Times(0);

  mojo::test::BadMessageObserver bad_message_observer;
  // Test fetching Google Photos photos before fetching the enterprise setting.
  // No requests should be made.
  wallpaper_provider_remote()->FetchGooglePhotosPhotos(
      item_id, album_id, kResumeToken,
      base::BindLambdaForTesting(
          [](mojom::FetchGooglePhotosPhotosResponsePtr response) {
            NOTREACHED_IN_MIGRATION();
          }));
  EXPECT_EQ(
      "Cannot call `FetchGooglePhotosPhotos()` without confirming that the "
      "Google Photos enterprise setting is enabled.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       SelectGooglePhotosPhoto) {
  test_wallpaper_controller()->ClearCounts();
  const std::string photo_id = "OmnisVirLupus";

  // Test selecting a wallpaper after fetching the enterprise setting.
  FetchGooglePhotosEnabled();
  base::test::TestFuture<bool> success_future;
  wallpaper_provider_remote()->SelectGooglePhotosPhoto(
      photo_id, ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  EXPECT_EQ(1,
            test_wallpaper_controller()->set_google_photos_wallpaper_count());
  EXPECT_TRUE(
      test_wallpaper_controller()
          ->wallpaper_info()
          .value_or(ash::WallpaperInfo())
          .MatchesSelection(ash::WallpaperInfo(
              {GetTestAccountId(), photo_id, /*daily_refresh_enabled=*/false,
               ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
               /*preview_mode=*/false, "dedup_key"})));
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       SelectGooglePhotosPhotoBeforeEnabled) {
  ASSERT_FALSE(test_wallpaper_controller()->wallpaper_info().has_value());

  mojo::test::BadMessageObserver bad_message_observer;
  // Test selecting a wallpaper before fetching the enterprise setting.
  wallpaper_provider_remote()->SelectGooglePhotosPhoto(
      "OmnisVirLupus", ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, base::BindLambdaForTesting([](bool success) {
        NOTREACHED_IN_MIGRATION();
      }));
  EXPECT_EQ(
      "Cannot call `SelectGooglePhotosPhoto()` without confirming that the "
      "Google Photos enterprise setting is enabled.",
      bad_message_observer.WaitForBadMessage());

  EXPECT_EQ(0,
            test_wallpaper_controller()->set_google_photos_wallpaper_count());
  EXPECT_FALSE(test_wallpaper_controller()->wallpaper_info().has_value());
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       SelectGooglePhotosPhotoBanned) {
  test_wallpaper_controller()->set_can_set_user_wallpaper(false);
  mojo::test::BadMessageObserver bad_message_observer;
  wallpaper_provider_remote()->SelectGooglePhotosPhoto(
      "OmnisVirLupus", ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, base::BindLambdaForTesting([](bool success) {
        NOTREACHED_IN_MIGRATION();
      }));
  EXPECT_EQ(
      "Cannot call `SelectGooglePhotosPhoto()` without confirming that the "
      "Google Photos enterprise setting is enabled.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       SelectGooglePhotosAlbumWithoutEnterprise) {
  // Test selecting an album before fetching the enterprise setting.
  mojo::test::BadMessageObserver bad_message_observer;
  wallpaper_provider_remote()->SelectGooglePhotosAlbum(
      "OmnisVirLupus", base::BindLambdaForTesting(
                           [](bool success) { NOTREACHED_IN_MIGRATION(); }));
  EXPECT_EQ(
      "Rejected attempt to set Google Photos wallpaper while disabled via "
      "enterprise setting.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       SelectGooglePhotosAlbum) {
  test_wallpaper_controller()->ClearCounts();
  EXPECT_EQ(test_wallpaper_controller()->GetGooglePhotosDailyRefreshAlbumId(
                GetTestAccountId()),
            "");
  const std::string album_id = "OmnisVirLupus";

  // Test selecting an album after fetching the enterprise setting.
  FetchGooglePhotosEnabled();
  base::test::TestFuture<bool> success_future;
  wallpaper_provider_remote()->SelectGooglePhotosAlbum(
      album_id, success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  EXPECT_EQ(test_wallpaper_controller()->GetGooglePhotosDailyRefreshAlbumId(
                GetTestAccountId()),
            album_id);
  EXPECT_EQ(
      test_wallpaper_controller()->get_update_daily_refresh_wallpaper_count(),
      1);
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       SelectGooglePhotosAlbumBanned) {
  test_wallpaper_controller()->set_can_set_user_wallpaper(false);
  FetchGooglePhotosEnabled();
  mojo::test::BadMessageObserver bad_message_observer;
  wallpaper_provider_remote()->SelectGooglePhotosAlbum(
      "OmnisVirLupus", base::BindLambdaForTesting(
                           [](bool success) { NOTREACHED_IN_MIGRATION(); }));
  EXPECT_EQ("Invalid request to select google photos album",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       SelectGooglePhotosAlbum_WithoutForcingRefresh) {
  FetchGooglePhotosEnabled();
  test_wallpaper_controller()->ClearCounts();
  const std::string album_id = "OmnisVirLupus";
  const std::string photo_id = "DummyPhotoId";

  // Test selecting a photo.
  {
    base::test::TestFuture<bool> success_future;
    wallpaper_provider_remote()->SelectGooglePhotosPhoto(
        photo_id, ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
        /*preview_mode=*/false, success_future.GetCallback());
    EXPECT_TRUE(success_future.Take());
  }

  // Add the same photo to a google photos album, and select that album as daily
  // refresh source.
  {
    AddToAlbumIdMap(album_id, photo_id);
    test_wallpaper_controller()->add_dedup_key_to_wallpaper_info(photo_id);
    base::test::TestFuture<bool> success_future;
    wallpaper_provider_remote()->SelectGooglePhotosAlbum(
        album_id, success_future.GetCallback());
    EXPECT_TRUE(success_future.Take());
    // Still equal to 0 since no need to update - already selected an image from
    // the album.
    EXPECT_EQ(
        test_wallpaper_controller()->get_update_daily_refresh_wallpaper_count(),
        0);
  }
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       UnselectGooglePhotosAlbum) {
  const std::string album_id = "OmnisVirLupus";
  const std::vector<std::string> photo_ids = {"id_zero", "id_one"};
  // Two photos in this album.
  for (const auto& photo_id : photo_ids) {
    AddToAlbumIdMap(album_id, photo_id);
  }

  FetchGooglePhotosEnabled();
  {
    // Select the album.
    base::test::TestFuture<bool> success_future;
    wallpaper_provider_remote()->SelectGooglePhotosAlbum(
        album_id, success_future.GetCallback());
    EXPECT_TRUE(success_future.Take());
    EXPECT_EQ(1, test_wallpaper_controller()
                     ->get_update_daily_refresh_wallpaper_count());
  }

  test_wallpaper_controller()->ClearCounts();
  {
    // Unselect the album.
    base::test::TestFuture<bool> success_future;
    wallpaper_provider_remote()->SelectGooglePhotosAlbum(
        std::string(), success_future.GetCallback());
    EXPECT_TRUE(success_future.Take());
    EXPECT_EQ(0, test_wallpaper_controller()
                     ->get_update_daily_refresh_wallpaper_count());
  }
}

TEST_F(PersonalizationAppWallpaperProviderImplGooglePhotosTest,
       UpdateDailyRefresh) {
  const std::string album_id = "OmnisVirLupus";
  const std::vector<std::string> photo_ids = {"id_zero", "id_one"};
  // Two photos in this album.
  for (const auto& photo_id : photo_ids) {
    AddToAlbumIdMap(album_id, photo_id);
  }

  FetchGooglePhotosEnabled();
  {
    base::test::TestFuture<bool> success_future;
    wallpaper_provider_remote()->SelectGooglePhotosAlbum(
        album_id, success_future.GetCallback());
    EXPECT_TRUE(success_future.Take());

    EXPECT_EQ(1, test_wallpaper_controller()
                     ->get_update_daily_refresh_wallpaper_count());
  }

  {
    base::test::TestFuture<bool> success_future;
    wallpaper_provider_remote()->UpdateDailyRefreshWallpaper(
        success_future.GetCallback());
    EXPECT_TRUE(success_future.Take());
    EXPECT_EQ(2, test_wallpaper_controller()
                     ->get_update_daily_refresh_wallpaper_count());
  }
}

}  // namespace ash::personalization_app
