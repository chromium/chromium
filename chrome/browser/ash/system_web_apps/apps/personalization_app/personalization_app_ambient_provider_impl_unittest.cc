// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_ambient_provider_impl.h"

#include <memory>
#include <string_view>
#include <vector>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/test/ambient_ash_test_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/personalization_app/time_of_day_test_utils.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/test_wallpaper_controller_client.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/ambient_video_albums.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_metrics.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace ash::personalization_app {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsFalse;
using ::testing::IsSupersetOf;
using ::testing::IsTrue;
using ::testing::Pointee;

constexpr char kFakeTestEmail[] = "fakeemail@example.com";
const AccountId kFakeTestAccountId =
    AccountId::FromUserEmailGaiaId(kFakeTestEmail, kFakeTestEmail);

class TestAmbientObserver
    : public ash::personalization_app::mojom::AmbientObserver {
 public:
  void OnAmbientModeEnabledChanged(bool ambient_mode_enabled) override {
    ambient_mode_enabled_ = ambient_mode_enabled;
  }

  void OnAmbientThemeChanged(mojom::AmbientTheme ambient_theme) override {
    ambient_theme_ = ambient_theme;
  }

  void OnTopicSourceChanged(mojom::TopicSource topic_source) override {
    topic_source_ = topic_source;
  }

  void OnAlbumsChanged(
      std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr> albums)
      override {
    albums_ = std::move(albums);
  }

  void OnScreenSaverDurationChanged(uint32_t minutes) override {
    duration_ = minutes;
  }

  void OnTemperatureUnitChanged(
      ash::AmbientModeTemperatureUnit temperature_unit) override {
    temperature_unit_ = temperature_unit;
  }

  void OnPreviewsFetched(const std::vector<GURL>& previews) override {
    previews_ = std::move(previews);
  }

  void OnAmbientUiVisibilityChanged(
      ash::AmbientUiVisibility visibility) override {
    ambient_ui_visibility_ = visibility;
  }

  void OnGeolocationPermissionForSystemServicesChanged(
      bool enabled,
      bool is_user_modifiable) override {
    geolocation_permission_enabled_ = enabled;
    is_geolocation_user_modifiable_ = is_user_modifiable;
  }

  mojo::PendingRemote<ash::personalization_app::mojom::AmbientObserver>
  pending_remote() {
    if (ambient_observer_receiver_.is_bound()) {
      ambient_observer_receiver_.reset();
    }

    return ambient_observer_receiver_.BindNewPipeAndPassRemote();
  }

  bool is_ambient_mode_enabled() {
    ambient_observer_receiver_.FlushForTesting();
    return ambient_mode_enabled_;
  }

  mojom::AmbientTheme ambient_theme() {
    ambient_observer_receiver_.FlushForTesting();
    return ambient_theme_;
  }

  mojom::TopicSource topic_source() {
    ambient_observer_receiver_.FlushForTesting();
    return topic_source_;
  }

  const std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr>&
  albums() {
    ambient_observer_receiver_.FlushForTesting();
    return albums_;
  }

  ash::AmbientModeTemperatureUnit temperature_unit() {
    ambient_observer_receiver_.FlushForTesting();
    return temperature_unit_;
  }

  ash::AmbientUiVisibility visibility() {
    ambient_observer_receiver_.FlushForTesting();
    return ambient_ui_visibility_;
  }

  std::vector<GURL> previews() {
    ambient_observer_receiver_.FlushForTesting();
    return previews_;
  }

  bool is_geolocation_enabled() {
    ambient_observer_receiver_.FlushForTesting();
    return geolocation_permission_enabled_;
  }

  bool is_geolocation_user_modifiable() {
    ambient_observer_receiver_.FlushForTesting();
    return is_geolocation_user_modifiable_;
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::AmbientObserver>
      ambient_observer_receiver_{this};

  bool ambient_mode_enabled_ = false;

  mojom::AmbientTheme ambient_theme_ = mojom::AmbientTheme::kSlideshow;
  uint32_t duration_ = 10;
  mojom::TopicSource topic_source_ = mojom::TopicSource::kArtGallery;
  ash::AmbientModeTemperatureUnit temperature_unit_ =
      ash::AmbientModeTemperatureUnit::kFahrenheit;
  ash::AmbientUiVisibility ambient_ui_visibility_ =
      ash::AmbientUiVisibility::kClosed;
  bool geolocation_permission_enabled_ = true;
  bool is_geolocation_user_modifiable_ = true;
  std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr> albums_;
  std::vector<GURL> previews_;
};

}  // namespace

class PersonalizationAppAmbientProviderImplTest : public ash::AshTestBase {
 public:
  PersonalizationAppAmbientProviderImplTest()
      : ash::AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures(
        personalization_app::GetTimeOfDayEnabledFeatures(), {});
  }
  PersonalizationAppAmbientProviderImplTest(
      const PersonalizationAppAmbientProviderImplTest&) = delete;
  PersonalizationAppAmbientProviderImplTest& operator=(
      const PersonalizationAppAmbientProviderImplTest&) = delete;
  ~PersonalizationAppAmbientProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    ash::AshTestBase::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);

    ash::FakeChromeUserManager* user_manager =
        static_cast<ash::FakeChromeUserManager*>(
            user_manager::UserManager::Get());
    user_manager->AddUser(kFakeTestAccountId);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    ambient_provider_ =
        std::make_unique<PersonalizationAppAmbientProviderImpl>(&web_ui_);

    ambient_provider_->BindInterface(
        ambient_provider_remote_.BindNewPipeAndPassReceiver());

    SetEnabledPref(true);
    GetAmbientAshTestHelper()->ambient_client().SetAutomaticalyIssueToken(true);

    Shell::Get()->ambient_controller()->set_backend_controller_for_testing(
        nullptr);

    fake_backend_controller_ =
        std::make_unique<ash::FakeAmbientBackendControllerImpl>();
  }

  void TearDown() override {
    // The PersonalizationAppAmbientProviderImpl holds a pointer to the
    // AmbientController the Shell owns (which is destructed in
    // AshTestBase::Teardown), so reset it first.
    ambient_provider_.reset();
    ash::AshTestBase::TearDown();
  }

  TestingProfile* profile() { return profile_; }

  mojo::Remote<ash::personalization_app::mojom::AmbientProvider>&
  ambient_provider_remote() {
    return ambient_provider_remote_;
  }

  content::TestWebUI* web_ui() { return &web_ui_; }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  void SetAmbientObserver() {
    ambient_provider_remote_->SetAmbientObserver(
        test_ambient_observer_.pending_remote());
  }

  bool ObservedAmbientModeEnabled() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.is_ambient_mode_enabled();
  }

  mojom::AmbientTheme ObservedAmbientTheme() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.ambient_theme();
  }

  mojom::TopicSource ObservedTopicSource() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.topic_source();
  }

  const std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr>&
  ObservedAlbums() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.albums();
  }

  ash::AmbientModeTemperatureUnit ObservedTemperatureUnit() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.temperature_unit();
  }

  ash::AmbientUiVisibility ObservedAmbientUiVisibility() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.visibility();
  }

  std::vector<GURL> ObservedPreviews() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.previews();
  }

  bool ObservedGeolocationPermissionEnabled() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.is_geolocation_enabled();
  }

  bool ObservedGeolocationIsManaged() {
    ambient_provider_remote_.FlushForTesting();
    return !test_ambient_observer_.is_geolocation_user_modifiable();
  }

  std::optional<ash::AmbientSettings>& settings() {
    return ambient_provider_->settings_;
  }

  void SetEnabledPref(bool enabled) {
    profile()->GetPrefs()->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled,
                                      enabled);
  }

  // Depending on the `managed` argument, sets the value of the
  // `kUserGeolocationAccessLevel` pref either in `PrefStoreType::MANAGED_STORE`
  // or in `PrefStoreType::USER_STORE` PrefStore.
  void SetGeolocationPref(bool enabled, bool managed) {
    GeolocationAccessLevel level;
    if (enabled) {
      level = GeolocationAccessLevel::kOnlyAllowedForSystem;
    } else {
      level = GeolocationAccessLevel::kDisallowed;
    }

    if (managed) {
      profile()->GetTestingPrefService()->SetManagedPref(
          ash::prefs::kUserGeolocationAccessLevel,
          base::Value(static_cast<int>(level)));
    } else {
      profile()->GetTestingPrefService()->SetUserPref(
          ash::prefs::kUserGeolocationAccessLevel,
          base::Value(static_cast<int>(level)));
    }
  }

  void SetAmbientTheme(mojom::AmbientTheme ambient_theme) {
    ambient_provider_->SetAmbientTheme(ambient_theme);
  }

  void FetchSettings() {
    ambient_provider_remote()->FetchSettingsAndAlbums();
    ambient_provider_remote().FlushForTesting();
  }

  void UpdateSettings() {
    if (!ambient_provider_->settings_) {
      ambient_provider_->settings_ = ash::AmbientSettings();
    }

    ambient_provider_->UpdateSettings();
  }

  void SetScreenSaverDuration(int minutes) {
    ambient_provider_->SetScreenSaverDuration(minutes);
  }

  void SetTopicSource(mojom::TopicSource topic_source) {
    ambient_provider_->SetTopicSource(topic_source);
  }

  void SetAlbumSelected(std::string_view id,
                        mojom::TopicSource topic_source,
                        bool selected) {
    ambient_provider_->SetAlbumSelected(std::string(id), topic_source,
                                        selected);
  }

  void FetchPreviewImages() { ambient_provider_->FetchPreviewImages(); }

  mojom::TopicSource TopicSource() {
    return ambient_provider_->settings_->topic_source;
  }

  std::vector<std::string> SelectedAlbumIds() {
    return ambient_provider_->settings_->selected_album_ids;
  }

  void SetSelectedAlbumIds(const std::vector<std::string>& ids) {
    ambient_provider_->settings_->selected_album_ids = ids;
  }

  void SetTemperatureUnit(ash::AmbientModeTemperatureUnit temperature_unit) {
    ambient_provider_->SetTemperatureUnit(temperature_unit);
  }

  ash::AmbientModeTemperatureUnit TemperatureUnit() {
    return ambient_provider_->settings_->temperature_unit;
  }

  std::vector<ash::ArtSetting> ArtSettings() {
    return ambient_provider_->settings_->art_settings;
  }

  bool IsUpdateSettingsPendingAtProvider() const {
    return ambient_provider_->is_updating_backend_;
  }

  base::TimeDelta GetFetchSettingsDelay() {
    return ambient_provider_->fetch_settings_retry_backoff_
        .GetTimeUntilRelease();
  }

  base::TimeDelta GetUpdateSettingsDelay() {
    return ambient_provider_->update_settings_retry_backoff_
        .GetTimeUntilRelease();
  }

  void FastForwardBy(base::TimeDelta time) {
    task_environment()->FastForwardBy(time);
  }

  bool IsFetchSettingsPendingAtBackend() const {
    return fake_backend_controller_->IsFetchSettingsAndAlbumsPending();
  }

  void ReplyFetchSettingsAndAlbums(
      bool success,
      std::optional<ash::AmbientSettings> settings = std::nullopt) {
    fake_backend_controller_->ReplyFetchSettingsAndAlbums(success,
                                                          std::move(settings));
  }

  bool IsUpdateSettingsPendingAtBackend() const {
    return fake_backend_controller_->IsUpdateSettingsPending();
  }

  void ReplyUpdateSettings(bool success) {
    fake_backend_controller_->ReplyUpdateSettings(success);
  }

  void EnableUpdateSettingsAutoReply(bool success) {
    fake_backend_controller_->EnableUpdateSettingsAutoReply(success);
  }

  AmbientModeTemperatureUnit GetCurrentTemperatureUnitInServer() const {
    return fake_backend_controller_->current_temperature_unit();
  }

  bool ShouldShowTimeOfDayBanner() const {
    base::test::TestFuture<bool> future;
    ambient_provider_remote_->ShouldShowTimeOfDayBanner(future.GetCallback());
    return future.Take();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<TestingProfile> profile_;
  mojo::Remote<ash::personalization_app::mojom::AmbientProvider>
      ambient_provider_remote_;
  std::unique_ptr<PersonalizationAppAmbientProviderImpl> ambient_provider_;
  TestAmbientObserver test_ambient_observer_;

  std::unique_ptr<ash::FakeAmbientBackendControllerImpl>
      fake_backend_controller_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PersonalizationAppAmbientProviderImplTest, IsAmbientModeEnabled) {
  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_TRUE(pref_service);
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, true);
  bool called = false;
  ambient_provider_remote()->IsAmbientModeEnabled(
      base::BindLambdaForTesting([&called](bool enabled) {
        called = true;
        EXPECT_TRUE(enabled);
      }));
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(called);

  called = false;
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, false);
  ambient_provider_remote()->IsAmbientModeEnabled(
      base::BindLambdaForTesting([&called](bool enabled) {
        called = true;
        EXPECT_FALSE(enabled);
      }));
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(called);
}

TEST_F(PersonalizationAppAmbientProviderImplTest, SetAmbientModeEnabled) {
  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_TRUE(pref_service);
  // Clear pref.
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, false);

  ambient_provider_remote()->SetAmbientModeEnabled(true);
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(
      pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled));

  ambient_provider_remote()->SetAmbientModeEnabled(false);
  ambient_provider_remote().FlushForTesting();
  EXPECT_FALSE(
      pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled));
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnAmbientModeEnabledChanged) {
  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_TRUE(pref_service);
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, false);
  SetAmbientObserver();
  FetchSettings();
  EXPECT_FALSE(ObservedAmbientModeEnabled());

  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, true);
  SetAmbientObserver();
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(ObservedAmbientModeEnabled());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       OnAmbientModeEnabled_ShouldCancelDelayedUpdateSettingsRequest) {
  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_TRUE(pref_service);
  UpdateSettings();
  // A failed response to UpdateSettings creates a new scheduled request to
  // UpdateSettings.
  ReplyUpdateSettings(/*success=*/false);

  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, false);

  base::TimeDelta delay1 = GetUpdateSettingsDelay();
  FastForwardBy(delay1 * 1.5);
  // Since ambient mode has been disabled, the pending update has been cleared.
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnAmbientThemeChanged) {
  // When ambient mode is first enabled during test set up, the video theme
  // should become active by default since the corresponding experiment flags
  // are on. That should count as +1 in the usage metrics for the video theme.
  histogram_tester().ExpectBucketCount(kAmbientModeAnimationThemeHistogramName,
                                       mojom::AmbientTheme::kVideo, 1);
  histogram_tester().ExpectBucketCount(kAmbientModeVideoHistogramName,
                                       ash::kDefaultAmbientVideo, 1);

  SetAmbientObserver();
  FetchSettings();
  SetAmbientTheme(mojom::AmbientTheme::kSlideshow);
  EXPECT_EQ(mojom::AmbientTheme::kSlideshow, ObservedAmbientTheme());
  histogram_tester().ExpectBucketCount(kAmbientModeAnimationThemeHistogramName,
                                       mojom::AmbientTheme::kSlideshow, 1);

  SetAmbientTheme(mojom::AmbientTheme::kFeelTheBreeze);
  EXPECT_EQ(mojom::AmbientTheme::kFeelTheBreeze, ObservedAmbientTheme());
  histogram_tester().ExpectBucketCount(kAmbientModeAnimationThemeHistogramName,
                                       mojom::AmbientTheme::kFeelTheBreeze, 1);

  SetAmbientTheme(mojom::AmbientTheme::kVideo);
  EXPECT_EQ(mojom::AmbientTheme::kVideo, ObservedAmbientTheme());
  histogram_tester().ExpectBucketCount(kAmbientModeAnimationThemeHistogramName,
                                       mojom::AmbientTheme::kVideo, 2);
  histogram_tester().ExpectBucketCount(kAmbientModeVideoHistogramName,
                                       ash::kDefaultAmbientVideo, 2);
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       RestoresOldThemeAfterReenabling) {
  SetAmbientObserver();
  FetchSettings();
  SetAmbientTheme(mojom::AmbientTheme::kFeelTheBreeze);
  SetEnabledPref(false);
  SetEnabledPref(true);
  EXPECT_EQ(mojom::AmbientTheme::kFeelTheBreeze, ObservedAmbientTheme());
  histogram_tester().ExpectBucketCount(kAmbientModeAnimationThemeHistogramName,
                                       mojom::AmbientTheme::kFeelTheBreeze, 2);
}

TEST_F(PersonalizationAppAmbientProviderImplTest, FetchPreviewImages) {
  SetAmbientObserver();
  EXPECT_TRUE(ObservedPreviews().empty());
  FetchPreviewImages();
  EXPECT_FALSE(ObservedPreviews().empty());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnTopicSourceChanged) {
  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  // The default theme is video theme.
  EXPECT_EQ(mojom::TopicSource::kVideo, ObservedTopicSource());
  EXPECT_FALSE(ObservedPreviews().empty());

  // The other topic sources do not apply to the video theme, so all other
  // `SetTopicSource()` calls should be rejected.
  SetTopicSource(mojom::TopicSource::kArtGallery);
  EXPECT_EQ(mojom::TopicSource::kVideo, ObservedTopicSource());
  SetTopicSource(mojom::TopicSource::kGooglePhotos);
  EXPECT_EQ(mojom::TopicSource::kVideo, ObservedTopicSource());

  // Set to a different theme and select different topic source.
  SetAmbientTheme(mojom::AmbientTheme::kSlideshow);
  EXPECT_EQ(mojom::TopicSource::kGooglePhotos, ObservedTopicSource());

  SetTopicSource(mojom::TopicSource::kArtGallery);
  EXPECT_EQ(mojom::TopicSource::kArtGallery, ObservedTopicSource());

  // The `kVideo` topic source is exclusive to the `kVideo` theme. It does not
  // apply to any of the other themes, so the existing topic source sticks.
  SetTopicSource(mojom::TopicSource::kVideo);
  EXPECT_EQ(mojom::TopicSource::kArtGallery, ObservedTopicSource());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, ShouldCallOnAlbumsChanged) {
  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  // The fake albums are set in FakeAmbientBackendControllerImpl. Hidden setting
  // will be sent to JS side.
  EXPECT_EQ(6u, ObservedAlbums().size());
  EXPECT_FALSE(ObservedPreviews().empty());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnTemperatureUnitChanged) {
  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            ObservedTemperatureUnit());
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            GetCurrentTemperatureUnitInServer());

  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kFahrenheit);
  ReplyUpdateSettings(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kFahrenheit,
            ObservedTemperatureUnit());
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kFahrenheit,
            GetCurrentTemperatureUnitInServer());

  // Even while the video topic source is active, temperature settings changes
  // should still be sent to the backend.
  SetAmbientTheme(mojom::AmbientTheme::kVideo);
  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kCelsius);
  ReplyUpdateSettings(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            ObservedTemperatureUnit());
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            GetCurrentTemperatureUnitInServer());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnAmbientUiVisibilityChanged) {
  SetAmbientObserver();
  EXPECT_EQ(ash::AmbientUiVisibility::kClosed, ObservedAmbientUiVisibility());
  Shell::Get()->ambient_controller()->ambient_ui_model()->SetUiVisibility(
      ash::AmbientUiVisibility::kPreview);
  EXPECT_EQ(ash::AmbientUiVisibility::kPreview, ObservedAmbientUiVisibility());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnGeolocationPermissionForSystemServicesChanged) {
  SetAmbientObserver();

  // Check default values:
  EXPECT_TRUE(ObservedGeolocationPermissionEnabled());
  EXPECT_FALSE(ObservedGeolocationIsManaged());

  // Check consumer scenario:
  SetGeolocationPref(/*enabled=*/false, /*managed=*/false);
  EXPECT_FALSE(ObservedGeolocationPermissionEnabled());
  EXPECT_FALSE(ObservedGeolocationIsManaged());
  SetGeolocationPref(/*enabled=*/true, /*managed=*/false);
  EXPECT_TRUE(ObservedGeolocationPermissionEnabled());
  EXPECT_FALSE(ObservedGeolocationIsManaged());

  // Check managed scenario:
  SetGeolocationPref(/*enabled=*/false, /*managed=*/true);
  EXPECT_FALSE(ObservedGeolocationPermissionEnabled());
  EXPECT_TRUE(ObservedGeolocationIsManaged());
  SetGeolocationPref(/*enabled=*/true, /*managed=*/true);
  EXPECT_TRUE(ObservedGeolocationPermissionEnabled());
  EXPECT_TRUE(ObservedGeolocationIsManaged());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, SetTopicSource) {
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  // Default screen saver is video theme with only kVideo topic source option.
  // Switch to other theme (kSlideshow) to try different topic sources.
  SetAmbientTheme(mojom::AmbientTheme::kSlideshow);

  EXPECT_EQ(mojom::TopicSource::kGooglePhotos, TopicSource());

  SetTopicSource(mojom::TopicSource::kArtGallery);
  EXPECT_EQ(mojom::TopicSource::kArtGallery, TopicSource());

  SetTopicSource(mojom::TopicSource::kGooglePhotos);
  EXPECT_EQ(mojom::TopicSource::kGooglePhotos, TopicSource());

  // If `settings_->selected_album_ids` is empty, will fallback to kArtGallery.
  SetSelectedAlbumIds(/*ids=*/{});
  SetTopicSource(mojom::TopicSource::kGooglePhotos);
  EXPECT_EQ(mojom::TopicSource::kArtGallery, TopicSource());

  SetSelectedAlbumIds(/*ids=*/{"1"});
  SetTopicSource(mojom::TopicSource::kGooglePhotos);
  EXPECT_EQ(mojom::TopicSource::kGooglePhotos, TopicSource());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, SetTemperatureUnit) {
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius, TemperatureUnit());

  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kFahrenheit);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kFahrenheit, TemperatureUnit());

  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kCelsius);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius, TemperatureUnit());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestFetchSettings) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestFetchSettingsFailedWillRetry) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestFetchSettingsSecondRetryWillBackoff) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  base::TimeDelta delay1 = GetFetchSettingsDelay();
  FastForwardBy(delay1 * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  base::TimeDelta delay2 = GetFetchSettingsDelay();
  EXPECT_GT(delay2, delay1);

  FastForwardBy(delay2 * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestFetchSettingsWillNotRetryMoreThanThreeTimes) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 1st retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 2nd retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 3rd retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // Will not retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestUpdateSettings) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());

  ReplyUpdateSettings(/*success=*/true);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestUpdateSettingsTwice_CancelsPreviousRequests) {
  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            ObservedTemperatureUnit());
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            GetCurrentTemperatureUnitInServer());

  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kFahrenheit);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());

  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kCelsius);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());

  ReplyUpdateSettings(/*success=*/true);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());

  // The newer temperature unit is used. The second call to UpdateSettings
  // cancels the first request.
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            ObservedTemperatureUnit());
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            GetCurrentTemperatureUnitInServer());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestUpdateSettingsFailedWillRetry) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());

  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestUpdateSettingsSecondRetryWillBackoff) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());

  base::TimeDelta delay1 = GetUpdateSettingsDelay();
  FastForwardBy(delay1 * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());

  base::TimeDelta delay2 = GetUpdateSettingsDelay();
  EXPECT_GT(delay2, delay1);

  FastForwardBy(delay2 * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestUpdateSettingsWillNotRetryMoreThanThreeTimes) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());

  // 1st retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());

  // 2nd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());

  // 3rd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());

  // Will not retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestNoFetchRequestWhenUpdatingSettings) {
  UpdateSettings();
  FetchSettings();

  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestSetSelectedGooglePhotosAlbum) {
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  // The fake data has album '1' as selected.
  std::vector<std::string> selected_ids = SelectedAlbumIds();
  EXPECT_TRUE(base::Contains(selected_ids, "1"));

  ash::personalization_app::mojom::AmbientModeAlbumPtr album =
      ash::personalization_app::mojom::AmbientModeAlbum::New();
  album->id = '1';
  album->topic_source = mojom::TopicSource::kGooglePhotos;
  album->checked = false;
  SetAlbumSelected(album->id, album->topic_source, album->checked);

  selected_ids = SelectedAlbumIds();
  EXPECT_TRUE(selected_ids.empty());
  // Will fallback to Art topic source if no selected Google Photos.
  EXPECT_EQ(mojom::TopicSource::kArtGallery, TopicSource());

  album = ash::personalization_app::mojom::AmbientModeAlbum::New();
  album->id = '1';
  album->topic_source = mojom::TopicSource::kGooglePhotos;
  album->checked = true;
  SetAlbumSelected(album->id, album->topic_source, album->checked);

  selected_ids = SelectedAlbumIds();
  EXPECT_EQ(1u, selected_ids.size());
  EXPECT_TRUE(base::Contains(selected_ids, "1"));
  EXPECT_EQ(mojom::TopicSource::kGooglePhotos, TopicSource());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestSetSelectedArtAlbum) {
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  // The fake data has art setting '0' as enabled.
  std::vector<ash::ArtSetting> art_settings = ArtSettings();
  auto it = base::ranges::find_if(art_settings, &ash::ArtSetting::enabled);
  EXPECT_NE(it, art_settings.end());
  EXPECT_EQ(it->album_id, "0");

  ash::personalization_app::mojom::AmbientModeAlbumPtr album =
      ash::personalization_app::mojom::AmbientModeAlbum::New();
  album->id = '0';
  album->topic_source = mojom::TopicSource::kArtGallery;
  album->checked = false;
  SetAlbumSelected(album->id, album->topic_source, album->checked);

  art_settings = ArtSettings();
  EXPECT_TRUE(base::ranges::none_of(art_settings, &ash::ArtSetting::enabled));

  album = ash::personalization_app::mojom::AmbientModeAlbum::New();
  album->id = '1';
  album->topic_source = mojom::TopicSource::kArtGallery;
  album->checked = true;
  SetAlbumSelected(album->id, album->topic_source, album->checked);

  art_settings = ArtSettings();
  it = base::ranges::find_if(art_settings, &ash::ArtSetting::enabled);
  EXPECT_NE(it, art_settings.end());
  EXPECT_EQ(it->album_id, "1");
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestSetSelectedVideo) {
  auto expect_videos_selected = [this](bool clouds_selected,
                                       bool new_mexico_select) {
    EXPECT_THAT(
        ObservedAlbums(),
        IsSupersetOf({Pointee(AllOf(Field(&mojom::AmbientModeAlbum::id,
                                          Eq(kCloudsAlbumId)),
                                    Field(&mojom::AmbientModeAlbum::checked,
                                          Eq(clouds_selected)))),
                      Pointee(AllOf(Field(&mojom::AmbientModeAlbum::id,
                                          Eq(kNewMexicoAlbumId)),
                                    Field(&mojom::AmbientModeAlbum::checked,
                                          Eq(new_mexico_select))))}));
  };

  // When ambient mode is first enabled during test set up, the video theme
  // should become active by default since the corresponding experiment flags
  // are on. That should count as +1 in the usage metrics for the video theme.
  histogram_tester().ExpectBucketCount(kAmbientModeVideoHistogramName,
                                       ash::AmbientVideo::kNewMexico, 1);

  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  // As Time of Day features are enabled, the default theme should be kVideo.
  EXPECT_EQ(ObservedTopicSource(), mojom::TopicSource::kVideo);

  // The default video should be checked.
  expect_videos_selected(/*clouds_selected=*/false,
                         /*new_mexico_selected=*/true);

  // Switch video to clouds.
  SetAlbumSelected(kCloudsAlbumId, mojom::TopicSource::kVideo, true);
  expect_videos_selected(/*clouds_selected=*/true,
                         /*new_mexico_selected=*/false);

  // Switch back to new mexico.
  SetAlbumSelected(kNewMexicoAlbumId, mojom::TopicSource::kVideo, true);
  expect_videos_selected(/*clouds_selected=*/false,
                         /*new_mexico_selected=*/true);

  // Should never be in a state where there are no videos selected.
  SetAlbumSelected(kNewMexicoAlbumId, mojom::TopicSource::kVideo, false);
  expect_videos_selected(/*clouds_selected=*/false,
                         /*new_mexico_selected=*/true);

  histogram_tester().ExpectBucketCount(kAmbientModeVideoHistogramName,
                                       ash::AmbientVideo::kNewMexico, 2);
  histogram_tester().ExpectBucketCount(kAmbientModeVideoHistogramName,
                                       ash::AmbientVideo::kClouds, 1);
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestAlbumNumbersAreRecorded) {
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  ash::personalization_app::mojom::AmbientModeAlbumPtr album =
      ash::personalization_app::mojom::AmbientModeAlbum::New();
  album->id = '0';
  album->topic_source = mojom::TopicSource::kGooglePhotos;
  SetAlbumSelected(album->id, album->topic_source, album->checked);
  histogram_tester().ExpectTotalCount("Ash.AmbientMode.TotalNumberOfAlbums",
                                      /*count=*/1);
  histogram_tester().ExpectTotalCount("Ash.AmbientMode.SelectedNumberOfAlbums",
                                      /*count=*/1);
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestEnabledPrefChangeUpdatesSettings) {
  // Simulate initial page request.
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  // Should not trigger |UpdateSettings|.
  SetEnabledPref(/*enabled=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  // Settings this to true should trigger |UpdateSettings|.
  SetEnabledPref(/*enabled=*/true);
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestWeatherFalseTriggersUpdateSettings) {
  ash::AmbientSettings weather_off_settings;
  weather_off_settings.show_weather = false;

  // Simulate initial page request with weather settings false. Because Ambient
  // mode pref is enabled and |settings.show_weather| is false, this should
  // trigger a call to |UpdateSettings| that sets |settings.show_weather| to
  // true.
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true, weather_off_settings);

  // A call to |UpdateSettings| should have happened.
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());

  ReplyUpdateSettings(/*success=*/true);

  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  // |settings.show_weather| should now be true after the successful settings
  // update.
  EXPECT_TRUE(settings()->show_weather);
}

// b/236723933
TEST_F(PersonalizationAppAmbientProviderImplTest,
       DoesNotCrashWithEmptyGooglePhotosAlbums) {
  SetEnabledPref(/*enabled=*/false);
  FetchSettings();
  // Reply with settings with |kGooglePhotos| but empty |selected_album_ids|.
  ash::AmbientSettings settings;
  settings.topic_source = mojom::TopicSource::kGooglePhotos;
  ReplyFetchSettingsAndAlbums(/*success=*/true,
                              /*settings=*/std::move(settings));
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       HandlesTransitionToFromVideoTopicSource) {
  // Start with the video topic source already active on boot.
  AmbientUiSettings(mojom::AmbientTheme::kVideo, AmbientVideo::kClouds)
      .WriteToPrefService(*profile()->GetPrefs());

  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  EXPECT_EQ(ObservedTopicSource(), mojom::TopicSource::kVideo);
  EXPECT_THAT(ObservedAlbums(),
              Contains(Pointee(
                  AllOf(Field(&mojom::AmbientModeAlbum::id, Eq(kCloudsAlbumId)),
                        Field(&mojom::AmbientModeAlbum::checked, IsTrue())))));

  // Switch to slide show mode and change settings to some custom configuration.
  SetAmbientTheme(mojom::AmbientTheme::kSlideshow);
  SetTopicSource(mojom::TopicSource::kArtGallery);
  SetAlbumSelected("1", mojom::TopicSource::kArtGallery, /*selected=*/true);
  ReplyUpdateSettings(/*success=*/true);

  // Switch back to video theme. Same video settings should remain.
  SetAmbientTheme(mojom::AmbientTheme::kVideo);
  EXPECT_EQ(ObservedTopicSource(), mojom::TopicSource::kVideo);
  EXPECT_THAT(ObservedAlbums(),
              Contains(Pointee(
                  AllOf(Field(&mojom::AmbientModeAlbum::id, Eq(kCloudsAlbumId)),
                        Field(&mojom::AmbientModeAlbum::checked, IsTrue())))));

  // Switch back to slide show. The custom setting set previously should stick.
  SetAmbientTheme(mojom::AmbientTheme::kSlideshow);
  EXPECT_EQ(ObservedTopicSource(), mojom::TopicSource::kArtGallery);
  EXPECT_THAT(ObservedAlbums(),
              Contains(Pointee(
                  AllOf(Field(&mojom::AmbientModeAlbum::id, Eq("1")),
                        Field(&mojom::AmbientModeAlbum::checked, IsTrue())))));
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       HandlesFailedSettingsUpdateForVideo) {
  EnableUpdateSettingsAutoReply(/*success=*/false);

  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  SetAmbientTheme(mojom::AmbientTheme::kVideo);
  // Let retries happen and try to expose any erroneous settings changes.
  task_environment()->FastForwardBy(base::Minutes(1));
  // Should not get stuck in a state where video theme is active with a
  // non-video topic source.
  ASSERT_EQ(ObservedAmbientTheme(), mojom::AmbientTheme::kVideo);
  EXPECT_EQ(ObservedTopicSource(), mojom::TopicSource::kVideo);
  EXPECT_THAT(ObservedAlbums(),
              Contains(Pointee(AllOf(
                  Field(&mojom::AmbientModeAlbum::id, Eq(kNewMexicoAlbumId)),
                  Field(&mojom::AmbientModeAlbum::checked, IsTrue())))));
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       HideBannerForPolicyManagedUsers) {
  WallpaperControllerImpl* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  base::ScopedTempDir user_wallpaper_dir;
  ASSERT_TRUE(user_wallpaper_dir.CreateUniqueTempDir());
  wallpaper_controller->Init(
      user_wallpaper_dir.GetPath(), user_wallpaper_dir.GetPath(),
      user_wallpaper_dir.GetPath(), user_wallpaper_dir.GetPath());
  TestWallpaperControllerClient client;
  wallpaper_controller->SetClient(&client);
  client.set_fake_files_id_for_account_id(kFakeTestAccountId,
                                          "wallpaper_files_id");
  wallpaper_controller->set_bypass_decode_for_testing();

  // Set default wallpaper for the user. Banner should be shown.
  wallpaper_controller->ShowDefaultWallpaperForTesting();
  ASSERT_FALSE(
      wallpaper_controller->IsWallpaperControlledByPolicy(kFakeTestAccountId));
  EXPECT_TRUE(ShouldShowTimeOfDayBanner());

  // Set policy managed wallpaper for the user. Banner should be hidden.
  wallpaper_controller->SetPolicyWallpaper(kFakeTestAccountId,
                                           user_manager::UserType::kRegular,
                                           std::string() /*data=*/);
  ASSERT_TRUE(
      wallpaper_controller->IsWallpaperControlledByPolicy(kFakeTestAccountId));
  EXPECT_FALSE(ShouldShowTimeOfDayBanner());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       DismissingBannerHidesItForever) {
  WallpaperControllerImpl* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  wallpaper_controller->set_bypass_decode_for_testing();
  wallpaper_controller->ShowDefaultWallpaperForTesting();
  ASSERT_FALSE(
      wallpaper_controller->IsWallpaperControlledByPolicy(kFakeTestAccountId));
  EXPECT_TRUE(ShouldShowTimeOfDayBanner());

  ambient_provider_remote()->HandleTimeOfDayBannerDismissed();

  EXPECT_FALSE(ShouldShowTimeOfDayBanner());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       UpdateSettingsFailure_ShowsCachedSettings) {
  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  // The cached settings have Celsius stored.
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            ObservedTemperatureUnit());
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            GetCurrentTemperatureUnitInServer());

  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kFahrenheit);

  // The value updates to Fahrenheit optimistically.
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kFahrenheit,
            ObservedTemperatureUnit());

  // Fail through all the retries.
  ReplyUpdateSettings(/*success=*/false);
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);

  // After all the failures, restore to the cached value.
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            ObservedTemperatureUnit());
}
}  // namespace ash::personalization_app
