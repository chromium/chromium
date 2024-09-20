// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/camera_effects_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/set_value_effects_view.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace ash {

using ::testing::ElementsAre;
using BackgroundImageInfo = CameraEffectsController::BackgroundImageInfo;

constexpr char kMetadataSuffix[] = ".metadata";

// Helper for converting `bitmap` into string.
std::string SkBitmapToString(const SkBitmap& bitmap) {
  std::vector<unsigned char> data;
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/100, &data);
  return std::string(data.begin(), data.end());
}

// Create fake Jpg image bytes.
std::string CreateJpgBytes(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(CameraEffectsController::kImageAsIconWidth,
                        CameraEffectsController::kImageAsIconWidth);
  bitmap.eraseColor(color);
  return SkBitmapToString(bitmap);
}

// Matcher defined to compare BackgroundImageInfo.
// We ignore the creation_time and last_accessed for now, because that were
// obtained from the filesystem, which is hard to mock for now.
auto BackgroundImageInfoMatcher(const base::FilePath& basename,
                                const std::string& jpeg_bytes,
                                const std::string& metadata) {
  return testing::AllOf(
      testing::Field("basename", &BackgroundImageInfo::basename,
                     testing::Eq(basename)),
      testing::ResultOf(
          [](BackgroundImageInfo info) {
            info.image.SetReadOnly();
            return SkBitmapToString(*info.image.bitmap());
          },
          testing::Eq(jpeg_bytes)),
      testing::Field("metadata", &BackgroundImageInfo::metadata,
                     testing::Eq(metadata)));
}

constexpr char kTestAccount[] = "testuser@gmail.com";
class CameraEffectsControllerTest : public NoSessionAshTestBase {
 public:
  // NoSessionAshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFeatureManagementVideoConference);

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    AshTestBase::SetUp();

    camera_effects_controller_ = Shell::Get()->camera_effects_controller();

    // Enable test mode to mock the SetCameraEffects calls.
    camera_effects_controller_->bypass_set_camera_effects_for_testing(true);

    // Create fake camera_background_img_dir_ and camera_background_run_dir_.
    ASSERT_TRUE(file_tmp_dir_.CreateUniqueTempDir());
    camera_background_img_dir_ =
        file_tmp_dir_.GetPath().AppendASCII("camera_background_img_dir_");
    camera_background_run_dir_ =
        file_tmp_dir_.GetPath().AppendASCII("camera_background_run_dir_");
    ASSERT_TRUE(base::CreateDirectory(camera_background_img_dir_));
    ASSERT_TRUE(base::CreateDirectory(camera_background_run_dir_));
  }

  void TearDown() override {
    NoSessionAshTestBase::TearDown();
    controller_.reset();
  }

  // Sets background blur state.
  void SetBackgroundBlurEffectState(std::optional<int> state) {
    camera_effects_controller_->OnEffectControlActivated(
        VcEffectId::kBackgroundBlur, state);
  }

  // Gets the state of the background blur effect from the effect's host,
  // `camera_effects_controller_`.
  int GetBackgroundBlurEffectState() {
    std::optional<int> effect_state =
        camera_effects_controller_->GetEffectState(VcEffectId::kBackgroundBlur);
    DCHECK(effect_state.has_value());
    return effect_state.value();
  }

  // Retrieves the value of `prefs::kBackgroundBlur`.
  int GetBackgroundBlurPref() {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetInteger(prefs::kBackgroundBlur);
  }

  // Returns a pair of <replace-enabled, background-image-filepath>.
  std::pair<bool, std::string> GetBackgroundReplacePref() {
    const auto* prefs =
        Shell::Get()->session_controller()->GetActivePrefService();

    return {prefs->GetBoolean(prefs::kBackgroundReplace),
            prefs->GetFilePath(prefs::kBackgroundImagePath).value()};
  }

  // Simulates toggling portrait relighting effect state. Note that the `state`
  // argument doesn't matter for toggle effects.
  void TogglePortraitRelightingEffectState() {
    camera_effects_controller_->OnEffectControlActivated(
        VcEffectId::kPortraitRelighting, /*state=*/std::nullopt);
  }

  // Simulates toggling camera framing effect state. Note that the `state`
  // argument doesn't matter for toggle effects.
  void ToggleCameraFramingEffectState() {
    camera_effects_controller_->OnEffectControlActivated(
        VcEffectId::kCameraFraming, /*state=*/std::nullopt);
  }

  // Gets the state of the portrait relighting effect from the effect's host,
  // `camera_effects_controller_`.
  bool GetPortraitRelightingEffectState() {
    std::optional<int> effect_state =
        camera_effects_controller_->GetEffectState(
            VcEffectId::kPortraitRelighting);
    DCHECK(effect_state.has_value());
    return static_cast<bool>(effect_state.value());
  }

  // Retrieves the value of `prefs::kPortraitRelighting`.
  bool GetPortraitRelightingPref() {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetBoolean(prefs::kPortraitRelighting);
  }

  void SetAutozoomSupportState(bool autozoom_supported) {
    auto* autozoom_controller = Shell::Get()->autozoom_controller();

    autozoom_controller->SetAutozoomSupported(autozoom_supported);

    if (!autozoom_supported) {
      return;
    }

    // Autozoom is only supported if there's an active camera client, so we
    // simulate that here.
    autozoom_controller->OnActiveClientChange(
        cros::mojom::CameraClientType::ASH_CHROME,
        /*is_new_active_client=*/true,
        /*active_device_ids=*/{"fake_id"});
  }

  CameraEffectsController* camera_effects_controller() {
    return camera_effects_controller_;
  }

  FakeVideoConferenceTrayController* tray_controller() {
    return controller_.get();
  }

  base::FilePath GetFileInBackgroundRunDir() {
    base::FileEnumerator enumerator(camera_background_run_dir_,
                                    /*recursive=*/false,
                                    base::FileEnumerator::FILES);
    std::vector<base::FilePath> files;
    for (auto path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      files.push_back(enumerator.GetInfo().GetName());
    }

    // We should always see 1 file in the camera_background_run_dir_.
    CHECK_EQ(files.size(), 1u);

    return files[0];
  }

 protected:
  const SeaPenImage content1_ =
      SeaPenImage(CreateJpgBytes(SK_ColorBLACK), 12345);
  const SeaPenImage content2_ = SeaPenImage(CreateJpgBytes(SK_ColorWHITE), 888);
  const std::string metadata1_ = "metadata1_";
  const std::string metadata2_ = "metadata2_";

  const base::FilePath filename1_ = base::FilePath("12345.jpg");
  const base::FilePath filename2_ = base::FilePath("888.jpg");
  const base::FilePath metadata_filename1_ =
      filename1_.AddExtensionASCII(kMetadataSuffix);
  base::FilePath metadata_filename2_ =
      filename2_.AddExtensionASCII(kMetadataSuffix);

  base::ScopedTempDir file_tmp_dir_;
  base::FilePath camera_background_img_dir_;
  base::FilePath camera_background_run_dir_;

  raw_ptr<CameraEffectsController, DanglingUntriaged>
      camera_effects_controller_ = nullptr;

  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
  base::test::ScopedFeatureList scoped_feature_list_;

  base::WeakPtrFactory<CameraEffectsControllerTest> weak_factory_{this};
};

TEST_F(CameraEffectsControllerTest, IsEffectControlAvailable) {
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {}, {features::kFeatureManagementVideoConference});
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kPortraitRelight));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundReplace));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kFeatureManagementVideoConference}, {});
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kPortraitRelight));
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundReplace));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kFeatureManagementVideoConference},
        {features::kVcPortraitRelight});
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kPortraitRelight));
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundReplace));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kFeatureManagementVideoConference},
        {features::kVcBackgroundReplace});
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kPortraitRelight));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundReplace));
  }
}

TEST_F(CameraEffectsControllerTest, BackgroundBlurOnEffectControlActivated) {
  SimulateUserLogin(kTestAccount);

  // Activate the possible values of
  // `CameraEffectsController::BackgroundBlurPrefValue`, verify that the pref
  //  and internal state are all set properly.
  for (const auto state :
       {CameraEffectsController::BackgroundBlurPrefValue::kOff,
        CameraEffectsController::BackgroundBlurPrefValue::kLowest,
        CameraEffectsController::BackgroundBlurPrefValue::kLight,
        CameraEffectsController::BackgroundBlurPrefValue::kMedium,
        CameraEffectsController::BackgroundBlurPrefValue::kHeavy,
        CameraEffectsController::BackgroundBlurPrefValue::kMaximum}) {
    SetBackgroundBlurEffectState(state);
    EXPECT_EQ(GetBackgroundBlurPref(), state);
    EXPECT_EQ(GetBackgroundBlurEffectState(), state);
  }

  // Invalid background blur effect state should set the state to kOff.
  SetBackgroundBlurEffectState(100);
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);

  // Set the background blur state to be kMaximum.
  SetBackgroundBlurEffectState(
      CameraEffectsController::BackgroundBlurPrefValue::kMaximum);
  // Setting the background blur state to null will reset the effects as
  // kOff.
  SetBackgroundBlurEffectState(std::nullopt);
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
}

TEST_F(CameraEffectsControllerTest,
       PortraitRelightingOnEffectControlActivated) {
  SimulateUserLogin(kTestAccount);

  // Initial state should be "off".
  EXPECT_FALSE(GetPortraitRelightingEffectState());
  EXPECT_FALSE(GetPortraitRelightingPref());

  // Activating the effect should toggle it to "true."
  TogglePortraitRelightingEffectState();
  EXPECT_TRUE(GetPortraitRelightingEffectState());
  EXPECT_TRUE(GetPortraitRelightingPref());

  // Another toggle should set it to "false."
  TogglePortraitRelightingEffectState();
  EXPECT_FALSE(GetPortraitRelightingEffectState());
  EXPECT_FALSE(GetPortraitRelightingPref());

  // And one more toggle should set it back to "true."
  TogglePortraitRelightingEffectState();
  EXPECT_TRUE(GetPortraitRelightingEffectState());
  EXPECT_TRUE(GetPortraitRelightingPref());
}

TEST_F(CameraEffectsControllerTest, PrefOnCameraEffectChanged) {
  SimulateUserLogin(kTestAccount);

  // Initial state should be "off".
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_FALSE(GetPortraitRelightingEffectState());
  EXPECT_FALSE(GetPortraitRelightingPref());

  // Case 1: when observe effects change from `CameraHalDispatcherImp`, the pref
  // is updated.
  cros::mojom::EffectsConfigPtr new_effects = cros::mojom::EffectsConfig::New();
  new_effects->blur_enabled = true;
  new_effects->blur_level = cros::mojom::BlurLevel::kMaximum;
  new_effects->relight_enabled = true;
  camera_effects_controller_->OnCameraEffectChanged(std::move(new_effects));

  // State should be "on".
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kMaximum);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kMaximum);
  EXPECT_TRUE(GetPortraitRelightingEffectState());
  EXPECT_TRUE(GetPortraitRelightingPref());

  // Case 2: when new effects is null, the pref is unchanged.
  new_effects = cros::mojom::EffectsConfigPtr();
  camera_effects_controller_->OnCameraEffectChanged(std::move(new_effects));

  // State should be "on".
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kMaximum);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kMaximum);
  EXPECT_TRUE(GetPortraitRelightingEffectState());
  EXPECT_TRUE(GetPortraitRelightingPref());

  // Case 3: when observe default effects from `CameraHalDispatcherImp`, the
  // pref should be back to default.
  new_effects = cros::mojom::EffectsConfig::New();
  camera_effects_controller_->OnCameraEffectChanged(std::move(new_effects));

  // State should be "off".
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_FALSE(GetPortraitRelightingEffectState());
  EXPECT_FALSE(GetPortraitRelightingPref());
}

TEST_F(CameraEffectsControllerTest, ResourceDependencyFlags) {
  SimulateUserLogin(kTestAccount);

  // Makes sure that all registered effects have the correct dependency flag.
  auto* background_blur =
      camera_effects_controller()->GetEffectById(VcEffectId::kBackgroundBlur);
  EXPECT_EQ(VcHostedEffect::ResourceDependency::kCamera,
            background_blur->dependency_flags());

  if (features::IsVcStudioLookEnabled()) {
    auto* studio_look =
        camera_effects_controller()->GetEffectById(VcEffectId::kStudioLook);
    EXPECT_EQ(VcHostedEffect::ResourceDependency::kCamera,
              studio_look->dependency_flags());
  } else {
    auto* portrait_relight = camera_effects_controller()->GetEffectById(
        VcEffectId::kPortraitRelighting);
    EXPECT_EQ(VcHostedEffect::ResourceDependency::kCamera,
              portrait_relight->dependency_flags());
  }
}

TEST_F(CameraEffectsControllerTest, BackgroundBlurEnums) {
  // This test makes sure that `BackgroundBlurState` and
  // `BackgroundBlurPrefValue` is in sync with each other.
  EXPECT_EQ(
      static_cast<int>(CameraEffectsController::BackgroundBlurState::kMaximum),
      CameraEffectsController::BackgroundBlurPrefValue::kMaximum + 1);
}

TEST_F(CameraEffectsControllerTest, BackgroundBlurMetricsRecord) {
  base::HistogramTester histogram_tester;

  SimulateUserLogin(kTestAccount);

  // Update media status to make the video conference tray visible.
  VideoConferenceMediaState state;
  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  state.is_capturing_screen = true;
  tray_controller()->UpdateWithMediaState(state);

  auto* vc_tray = StatusAreaWidgetTestHelper::GetStatusAreaWidget()
                      ->video_conference_tray();

  // Open the vc bubble.
  LeftClickOn(vc_tray->toggle_bubble_button());

  // The set-value effects panel should have only 1 child view, and that view
  // should be the slider associated to the background blur effect.
  auto* set_value_effects_view = vc_tray->GetBubbleView()->GetViewByID(
      video_conference::BubbleViewID::kSetValueEffectsView);

  ASSERT_EQ(1u, set_value_effects_view->children().size());

  auto* background_blur_slider =
      static_cast<video_conference::SetValueEffectSlider*>(
          set_value_effects_view->children()[0]);
  EXPECT_EQ(VcEffectId::kBackgroundBlur, background_blur_slider->effect_id());

  auto* tab_slider = background_blur_slider->tab_slider();
  auto* first_button = tab_slider->GetButtonAtIndex(0);

  // At first, the first button is selected, but there should not be any metrics
  // recorded for that state since it is the default state when opening the
  // bubble.
  EXPECT_TRUE(first_button->selected());
  histogram_tester.ExpectBucketCount(
      "Ash.VideoConferenceTray.BackgroundBlur.Click",
      CameraEffectsController::BackgroundBlurState::kOff, 0);

  // Switching states by clicking slider buttons should record metrics in the
  // associated bucket.
  LeftClickOn(tab_slider->GetButtonAtIndex(1));

  histogram_tester.ExpectBucketCount(
      "Ash.VideoConferenceTray.BackgroundBlur.Click",
      CameraEffectsController::BackgroundBlurState::kLight, 1);

  LeftClickOn(tab_slider->GetButtonAtIndex(2));

  histogram_tester.ExpectBucketCount(
      "Ash.VideoConferenceTray.BackgroundBlur.Click",
      CameraEffectsController::BackgroundBlurState::kMaximum, 1);

  LeftClickOn(first_button);

  histogram_tester.ExpectBucketCount(
      "Ash.VideoConferenceTray.BackgroundBlur.Click",
      CameraEffectsController::BackgroundBlurState::kOff, 1);
}

TEST_F(CameraEffectsControllerTest, CameraFramingSupportState) {
  SimulateUserLogin(kTestAccount);

  // By default autozoom is not supported, so the effect is not added.
  EXPECT_FALSE(
      camera_effects_controller()->GetEffectById(VcEffectId::kCameraFraming));

  SetAutozoomSupportState(true);

  EXPECT_TRUE(
      camera_effects_controller()->GetEffectById(VcEffectId::kCameraFraming));

  SetAutozoomSupportState(false);

  EXPECT_FALSE(
      camera_effects_controller()->GetEffectById(VcEffectId::kCameraFraming));
}

TEST_F(CameraEffectsControllerTest, CameraFramingToggle) {
  SetAutozoomSupportState(true);

  SimulateUserLogin(kTestAccount);

  ASSERT_EQ(Shell::Get()->autozoom_controller()->GetState(),
            cros::mojom::CameraAutoFramingState::OFF);

  ToggleCameraFramingEffectState();
  EXPECT_EQ(Shell::Get()->autozoom_controller()->GetState(),
            cros::mojom::CameraAutoFramingState::ON_SINGLE);

  ToggleCameraFramingEffectState();
  EXPECT_EQ(Shell::Get()->autozoom_controller()->GetState(),
            cros::mojom::CameraAutoFramingState::OFF);
}

TEST_F(CameraEffectsControllerTest, SetBackgroundImageWithFileExists) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kVcBackgroundReplace};

  SimulateUserLogin(kTestAccount);
  camera_effects_controller()->set_camera_background_img_dir_for_testing(
      camera_background_img_dir_);
  camera_effects_controller()->set_camera_background_run_dir_for_testing(
      camera_background_run_dir_);

  // Apply background blur first.
  const auto state = CameraEffectsController::BackgroundBlurPrefValue::kLowest;
  SetBackgroundBlurEffectState(state);
  EXPECT_EQ(GetBackgroundBlurPref(), state);

  // Create fake image file.
  const std::string relative_path = "background/test.png";
  base::FilePath file_fullpath =
      camera_background_img_dir_.Append(relative_path);
  ASSERT_TRUE(base::CreateDirectory(file_fullpath.DirName()));
  ASSERT_TRUE(base::WriteFile(file_fullpath, ""));

  // Set background image.
  camera_effects_controller()->SetBackgroundImage(
      base::FilePath(relative_path),
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // Check background replace result from pref.
  EXPECT_THAT(GetBackgroundReplacePref(), testing::Pair(true, relative_path));

  // Check the background blur is turned off.
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);

  // Apply background blur again.
  SetBackgroundBlurEffectState(state);
  EXPECT_EQ(GetBackgroundBlurPref(), state);

  // Background replace should be turned off.
  EXPECT_THAT(GetBackgroundReplacePref(), testing::Pair(false, ""));

  // When background replace is turned off, we want the background_filepath to
  // be empty.
  EXPECT_FALSE(camera_effects_controller()
                   ->GetCameraEffects()
                   ->background_filepath.has_value());

  // Set background image again.
  camera_effects_controller()->SetBackgroundImage(
      base::FilePath(relative_path),
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // Check background replace result from pref.
  EXPECT_THAT(GetBackgroundReplacePref(), testing::Pair(true, relative_path));

  // Turn off backgroundblur or replace.
  const auto off_state = CameraEffectsController::BackgroundBlurPrefValue::kOff;
  SetBackgroundBlurEffectState(off_state);
  EXPECT_EQ(GetBackgroundBlurPref(), off_state);

  // Background replace should be turned off.
  EXPECT_THAT(GetBackgroundReplacePref(), testing::Pair(false, ""));
}

TEST_F(CameraEffectsControllerTest, SetBackgroundImageWithFileDoesNotExist) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kVcBackgroundReplace};

  SimulateUserLogin(kTestAccount);
  camera_effects_controller()->set_camera_background_img_dir_for_testing(
      camera_background_img_dir_);
  camera_effects_controller()->set_camera_background_run_dir_for_testing(
      camera_background_run_dir_);

  // Apply background blur first.
  const auto state = CameraEffectsController::BackgroundBlurPrefValue::kLowest;
  SetBackgroundBlurEffectState(state);
  EXPECT_EQ(GetBackgroundBlurPref(), state);

  // Set background image.
  camera_effects_controller()->SetBackgroundImage(
      filename1_, base::BindOnce([](bool call_succeeded) {
        EXPECT_FALSE(call_succeeded);
      }));
  task_environment()->RunUntilIdle();

  // Because the image is not created, the above SetBackgroundImage should fail,
  // so that the background replace pref should not be set.
  EXPECT_THAT(GetBackgroundReplacePref(), testing::Pair(false, ""));

  // Check the background blur is not changed.
  EXPECT_EQ(GetBackgroundBlurPref(), state);
}

TEST_F(CameraEffectsControllerTest, SetBackgroundImageFromContent) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kVcBackgroundReplace};

  SimulateUserLogin(kTestAccount);
  camera_effects_controller()->set_camera_background_img_dir_for_testing(
      camera_background_img_dir_);
  camera_effects_controller()->set_camera_background_run_dir_for_testing(
      camera_background_run_dir_);

  // Set background image from content1_.
  camera_effects_controller()->SetBackgroundImageFromContent(
      content1_, metadata1_,
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // We should see replace-enabled and the filename1_ in prefs.
  EXPECT_THAT(GetBackgroundReplacePref(),
              testing::Pair(true, filename1_.value()));

  // Check saved image info.
  camera_effects_controller()->GetRecentlyUsedBackgroundImages(
      3, base::BindLambdaForTesting(
             [&](const std::vector<BackgroundImageInfo>& info) {
               EXPECT_THAT(info,
                           ElementsAre(BackgroundImageInfoMatcher(
                               filename1_, content1_.jpg_bytes, metadata1_)));
               EXPECT_EQ(GetFileInBackgroundRunDir(), filename1_);
             }));
  task_environment()->RunUntilIdle();

  // Set background image from content2_.
  camera_effects_controller()->SetBackgroundImageFromContent(
      content2_, metadata2_,
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // We should see replace-enabled and the filename2_ in prefs.
  EXPECT_THAT(GetBackgroundReplacePref(),
              testing::Pair(true, filename2_.value()));

  camera_effects_controller()->GetRecentlyUsedBackgroundImages(
      3, base::BindLambdaForTesting(
             [&](const std::vector<BackgroundImageInfo>& info) {
               EXPECT_THAT(
                   info, ElementsAre(
                             BackgroundImageInfoMatcher(
                                 filename2_, content2_.jpg_bytes, metadata2_),
                             BackgroundImageInfoMatcher(
                                 filename1_, content1_.jpg_bytes, metadata1_)));
               EXPECT_EQ(GetFileInBackgroundRunDir(), filename2_);
             }));
  task_environment()->RunUntilIdle();

  // SetBackgroundImage with filename1_ should update the last activity
  // time of filename1_.
  camera_effects_controller()->SetBackgroundImage(
      filename1_,
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // We should see replace-enabled and the filename1_ in prefs.
  EXPECT_THAT(GetBackgroundReplacePref(),
              testing::Pair(true, filename1_.value()));

  camera_effects_controller()->GetRecentlyUsedBackgroundImages(
      3, base::BindLambdaForTesting(
             [&](const std::vector<BackgroundImageInfo>& info) {
               EXPECT_THAT(
                   info, ElementsAre(
                             BackgroundImageInfoMatcher(
                                 filename1_, content1_.jpg_bytes, metadata1_),
                             BackgroundImageInfoMatcher(
                                 filename2_, content2_.jpg_bytes, metadata2_)));
               EXPECT_EQ(GetFileInBackgroundRunDir(), filename1_);
             }));
  task_environment()->RunUntilIdle();

  // Remove filename2_.
  camera_effects_controller()->RemoveBackgroundImage(
      filename2_,
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // Pref should not be effect if a irrelevant image is removed.
  EXPECT_THAT(GetBackgroundReplacePref(),
              testing::Pair(true, filename1_.value()));

  camera_effects_controller()->GetRecentlyUsedBackgroundImages(
      3, base::BindLambdaForTesting(
             [&](const std::vector<BackgroundImageInfo>& info) {
               EXPECT_THAT(info,
                           ElementsAre(BackgroundImageInfoMatcher(
                               filename1_, content1_.jpg_bytes, metadata1_)));
             }));
  task_environment()->RunUntilIdle();

  // Remove filename1_.
  camera_effects_controller()->RemoveBackgroundImage(
      filename1_,
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // Since filename2_ is removed, we should see the background image is removed
  // from prefs.
  EXPECT_THAT(GetBackgroundReplacePref(), testing::Pair(false, ""));

  camera_effects_controller()->GetRecentlyUsedBackgroundImages(
      3, base::BindLambdaForTesting(
             [&](const std::vector<BackgroundImageInfo>& info) {
               // We should only see all files removed.
               EXPECT_TRUE(info.empty());
             }));
  task_environment()->RunUntilIdle();
}

TEST_F(CameraEffectsControllerTest, GetBackgroundImageFileNames) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kVcBackgroundReplace};

  SimulateUserLogin(kTestAccount);
  camera_effects_controller()->set_camera_background_img_dir_for_testing(
      camera_background_img_dir_);
  camera_effects_controller()->set_camera_background_run_dir_for_testing(
      camera_background_run_dir_);

  // Set background image from content1_.
  camera_effects_controller()->SetBackgroundImageFromContent(
      content1_, metadata1_,
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // filename1_ should be created.
  camera_effects_controller()->GetBackgroundImageFileNames(
      base::BindLambdaForTesting([&](const std::vector<base::FilePath>& files) {
        EXPECT_THAT(files, ElementsAre(filename1_));
      }));
  task_environment()->RunUntilIdle();

  // Set background image from content2_.
  camera_effects_controller()->SetBackgroundImageFromContent(
      content2_, metadata2_,
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // filename2_ should be created.
  camera_effects_controller()->GetBackgroundImageFileNames(
      base::BindLambdaForTesting([&](const std::vector<base::FilePath>& files) {
        EXPECT_THAT(files, ElementsAre(filename2_, filename1_));
      }));
  task_environment()->RunUntilIdle();

  // SetBackgroundImage with filename1_ should update the last activity
  // time of filename1_.
  camera_effects_controller()->SetBackgroundImage(
      filename1_,
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // Returned files order should be changed.
  camera_effects_controller()->GetBackgroundImageFileNames(
      base::BindLambdaForTesting([&](const std::vector<base::FilePath>& files) {
        EXPECT_THAT(files, ElementsAre(filename1_, filename2_));
      }));
  task_environment()->RunUntilIdle();

  // Remove filename1_.
  camera_effects_controller()->RemoveBackgroundImage(
      filename1_,
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // We should only see filename2_.
  camera_effects_controller()->GetBackgroundImageFileNames(
      base::BindLambdaForTesting([&](const std::vector<base::FilePath>& files) {
        EXPECT_THAT(files, ElementsAre(filename2_));
      }));
  task_environment()->RunUntilIdle();
}

TEST_F(CameraEffectsControllerTest, GetBackgroundImageInfo) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kVcBackgroundReplace};

  SimulateUserLogin(kTestAccount);
  camera_effects_controller()->set_camera_background_img_dir_for_testing(
      camera_background_img_dir_);
  camera_effects_controller()->set_camera_background_run_dir_for_testing(
      camera_background_run_dir_);

  // Set background image from content1_.
  camera_effects_controller()->SetBackgroundImageFromContent(
      content1_, metadata1_,
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  // GetBackgroundImageInfo should return info about filename1_.
  camera_effects_controller()->GetBackgroundImageInfo(
      filename1_, base::BindLambdaForTesting(
                      [&](const std::optional<BackgroundImageInfo>& info) {
                        EXPECT_THAT(
                            info.value(),
                            BackgroundImageInfoMatcher(
                                filename1_, content1_.jpg_bytes, metadata1_));
                      }));
  task_environment()->RunUntilIdle();

  // Delete metadata_filename1_ will return empty metadata.
  camera_effects_controller()->RemoveBackgroundImage(
      metadata_filename1_,
      base::BindOnce([](bool call_succeeded) { EXPECT_TRUE(call_succeeded); }));
  task_environment()->RunUntilIdle();

  camera_effects_controller()->GetBackgroundImageInfo(
      filename1_, base::BindLambdaForTesting(
                      [&](const std::optional<BackgroundImageInfo>& info) {
                        EXPECT_THAT(info.value(),
                                    BackgroundImageInfoMatcher(
                                        filename1_, content1_.jpg_bytes, ""));
                      }));
  task_environment()->RunUntilIdle();

  // GetBackgroundImageInfo should return nullopt for filename2_ because the
  // file is not created.
  camera_effects_controller()->GetBackgroundImageInfo(
      filename2_,
      base::BindOnce([](const std::optional<BackgroundImageInfo>& info) {
        EXPECT_FALSE(info.has_value());
      }));
  task_environment()->RunUntilIdle();
}

TEST_F(CameraEffectsControllerTest, NotEligibleForSeaPen) {
  // Set is_eligible_for_background_replace to false so that the image button
  // will not be constructed.
  GetSessionControllerClient()->set_is_eligible_for_background_replace(
      {false, false});
  SimulateUserLogin(kTestAccount);

  // Update media status to make the video conference tray visible.
  VideoConferenceMediaState state;
  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  state.is_capturing_screen = true;
  tray_controller()->UpdateWithMediaState(state);

  auto effects = VideoConferenceTrayController::Get()
                     ->GetEffectsManager()
                     .GetSetValueEffects();

  EXPECT_EQ(effects.size(), 1u);
  EXPECT_EQ(effects[0]->label_text(), u"Background");
  // Verify that only three states are constructed; the forth one is the image
  // button.
  EXPECT_EQ(effects[0]->GetNumStates(), 3);
}

TEST_F(CameraEffectsControllerTest, UpdateBackgroundBlurImageState) {
  // Set is_eligible_for_background_replace to false so that the image button
  // will not be constructed.
  GetSessionControllerClient()->set_is_eligible_for_background_replace(
      {false, false});
  SimulateUserLogin(kTestAccount);

  // Update media status to make the video conference tray visible.
  VideoConferenceMediaState state;
  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  state.is_capturing_screen = true;
  tray_controller()->UpdateWithMediaState(state);

  auto effects = VideoConferenceTrayController::Get()
                     ->GetEffectsManager()
                     .GetSetValueEffects();

  EXPECT_EQ(effects.size(), 1u);
  EXPECT_EQ(effects[0]->label_text(), u"Background");
  // Verify that only three states are constructed; the forth one is the image
  // button.
  EXPECT_EQ(effects[0]->GetNumStates(), 3);

  // Set background replace eligible state to true and enterprise enabled state
  // to false so that the image button is added but disabled.
  GetSessionControllerClient()->set_is_eligible_for_background_replace(
      {true, false});
  auto* vc_tray = StatusAreaWidgetTestHelper::GetStatusAreaWidget()
                      ->video_conference_tray();

  // Open the vc bubble to notify bubble opened and update Background Blur
  // effect.
  LeftClickOn(vc_tray->toggle_bubble_button());

  effects = VideoConferenceTrayController::Get()
                ->GetEffectsManager()
                .GetSetValueEffects();

  // Now four states are constructed and the forth one is the image button.
  EXPECT_EQ(effects[0]->GetNumStates(), 4) << " four states are constructed";
  const VcEffectState* imageState = effects[0]->GetState(/*index=*/3);
  EXPECT_EQ(imageState->view_id(),
            video_conference::BubbleViewID::kBackgroundBlurImageButton);
  EXPECT_TRUE(imageState->is_disabled_by_enterprise());

  // Update VC Background enterprise enabled state to true so that the Image
  // button is enabled.
  GetSessionControllerClient()->set_is_eligible_for_background_replace(
      {true, true});
  // Close the video conference bubble.
  LeftClickOn(vc_tray->toggle_bubble_button());
  // Reopen the bubble to trigger updating Background Blur effect again.
  LeftClickOn(vc_tray->toggle_bubble_button());

  effects = VideoConferenceTrayController::Get()
                ->GetEffectsManager()
                .GetSetValueEffects();

  // The image button is now enabled.
  EXPECT_EQ(effects[0]->GetNumStates(), 4)
      << "still four states for Background Blur effect";
  const VcEffectState* newImageState = effects[0]->GetState(/*index=*/3);
  EXPECT_EQ(newImageState->view_id(),
            video_conference::BubbleViewID::kBackgroundBlurImageButton);
  EXPECT_FALSE(newImageState->is_disabled_by_enterprise());
}

}  // namespace ash
