// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/bubble_view.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"

namespace ash::video_conference {

namespace {

constexpr int kSquareCinnamonCerealViewId = 1235;
constexpr int kSnackNationForeverViewId = 1236;

// A fake `kToggle` effect.
class SquareCinnamonCereal : public VcEffectsDelegate {
 public:
  explicit SquareCinnamonCereal(
      VcHostedEffect::ResourceDependencyFlags dependency_flags) {
    auto effect = std::make_unique<VcHostedEffect>(
        VcEffectType::kToggle,
        base::BindRepeating(&SquareCinnamonCereal::GetEffectState,
                            base::Unretained(this),
                            /*effect_id=*/VcEffectId::kTestEffect),
        VcEffectId::kTestEffect);
    effect->set_container_id(kSquareCinnamonCerealViewId);
    effect->set_dependency_flags(dependency_flags);

    auto state = std::make_unique<VcEffectState>(
        &kVideoConferenceBackgroundBlurMaximumIcon, u"Square Cinnamon Cereal",
        IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
        base::BindRepeating(&SquareCinnamonCereal::OnEffectControlActivated,
                            base::Unretained(this),
                            /*effect_id=*/VcEffectId::kTestEffect,
                            /*value=*/absl::nullopt));
    state->set_disabled_icon(&kVideoConferenceBackgroundBlurOffIcon);
    effect->AddState(std::move(state));

    AddEffect(std::move(effect));
  }
  SquareCinnamonCereal(const SquareCinnamonCereal&) = delete;
  SquareCinnamonCereal& operator=(const SquareCinnamonCereal&) = delete;
  ~SquareCinnamonCereal() override = default;

  // VcEffectsDelegate:
  absl::optional<int> GetEffectState(VcEffectId effect_id) override {
    return 0;
  }
  void OnEffectControlActivated(VcEffectId effect_id,
                                absl::optional<int> state) override {}
};

// A fake `kSetValue` effect.
class SnackNationForever : public VcEffectsDelegate {
 public:
  explicit SnackNationForever(
      VcHostedEffect::ResourceDependencyFlags dependency_flags) {
    auto effect = std::make_unique<VcHostedEffect>(
        VcEffectType::kSetValue,
        base::BindRepeating(&SnackNationForever::GetEffectState,
                            base::Unretained(this),
                            /*effect_id=*/VcEffectId::kTestEffect),
        VcEffectId::kTestEffect);
    auto state = std::make_unique<VcEffectState>(
        &ash::kPrivacyIndicatorsCameraIcon, u"Snack Nation",
        IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
        base::BindRepeating(&SnackNationForever::OnEffectControlActivated,
                            base::Unretained(this),
                            /*effect_id=*/VcEffectId::kTestEffect,
                            /*value=*/0),
        /*state=*/0);
    effect->AddState(std::move(state));
    effect->set_container_id(kSnackNationForeverViewId);
    effect->set_dependency_flags(dependency_flags);
    AddEffect(std::move(effect));
  }
  SnackNationForever(const SnackNationForever&) = delete;
  SnackNationForever& operator=(const SnackNationForever&) = delete;
  ~SnackNationForever() override = default;

  // VcEffectsDelegate:
  absl::optional<int> GetEffectState(VcEffectId effect_id) override {
    return 0;
  }
  void OnEffectControlActivated(VcEffectId effect_id,
                                absl::optional<int> state) override {}
};

}  // namespace

class BubbleViewTest : public AshTestBase {
 public:
  BubbleViewTest() = default;
  BubbleViewTest(const BubbleViewTest&) = delete;
  BubbleViewTest& operator=(const BubbleViewTest&) = delete;
  ~BubbleViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVideoConference);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCameraEffectsSupportedByHardware);

    // Instantiates a fake controller (the real one is created in
    // `ChromeBrowserMainExtraPartsAsh::PreProfileInit()` which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    // Instantiate these fake effects, to be registered/unregistered as needed.
    office_bunny_ =
        std::make_unique<fake_video_conference::OfficeBunnyEffect>();
    shaggy_fur_ = std::make_unique<fake_video_conference::ShaggyFurEffect>();
    super_cuteness_ =
        std::make_unique<fake_video_conference::SuperCutnessEffect>();

    AshTestBase::SetUp();

    // Make the video conference tray visible for testing.
    video_conference_tray()->SetVisiblePreferred(true);

    // For historical reason, all BubbleViewTest tests are written with the
    // assumption that CameraEffectsController is not registered to the
    // EffectsManager by default. It is not the case anymore since we removed
    // the old Flags. The fix for that is easy: we just need to manually
    // unregister CameraEffectsController in these tests.
    controller()->effects_manager().UnregisterDelegate(
        Shell::Get()->camera_effects_controller());
  }

  void TearDown() override {
    AshTestBase::TearDown();
    office_bunny_.reset();
    shaggy_fur_.reset();
    super_cuteness_.reset();
    controller_.reset();
  }

  views::View* GetSetValueEffectButton(int index) {
    // Map `index` to a `BubbleViewID`, for lookup.
    BubbleViewID id =
        static_cast<BubbleViewID>(index + BubbleViewID::kSetValueButtonMin);
    DCHECK_GE(id, BubbleViewID::kSetValueButtonMin);
    DCHECK_LE(id, BubbleViewID::kSetValueButtonMax);
    return bubble_view()->GetViewByID(id);
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  IconButton* toggle_bubble_button() {
    return video_conference_tray()->toggle_bubble_button_;
  }

  views::View* bubble_view() {
    return video_conference_tray()->GetBubbleView();
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

  views::View* toggle_effects_view() {
    return bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kToggleEffectsView);
  }

  views::View* set_value_effects_view() {
    return bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kSetValueEffectsView);
  }

  views::View* single_set_value_effect_view() {
    return bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kSingleSetValueEffectView);
  }

  views::View* return_to_app() {
    return bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kReturnToApp);
  }

  views::View* toggle_effect_button() {
    return bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kToggleEffectsButton);
  }

  ash::fake_video_conference::OfficeBunnyEffect* office_bunny() {
    return office_bunny_.get();
  }

  ash::fake_video_conference::ShaggyFurEffect* shaggy_fur() {
    return shaggy_fur_.get();
  }

  ash::fake_video_conference::SuperCutnessEffect* super_cuteness() {
    return super_cuteness_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
  std::unique_ptr<ash::fake_video_conference::OfficeBunnyEffect> office_bunny_;
  std::unique_ptr<ash::fake_video_conference::ShaggyFurEffect> shaggy_fur_;
  std::unique_ptr<ash::fake_video_conference::SuperCutnessEffect>
      super_cuteness_;
};

// TODO(b/273604669): Move tests that are only related to `toggle_effects_view`
// to its own unit test file.
TEST_F(BubbleViewTest, NoEffects) {
  EXPECT_FALSE(bubble_view());

  // Clicking the toggle button should construct and open up the bubble.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(bubble_view());
  EXPECT_TRUE(bubble_view()->GetVisible());

  // "Return to app" is present and visible.
  EXPECT_TRUE(return_to_app());
  EXPECT_TRUE(return_to_app()->GetVisible());

  // No effects added, no effects view(s) present.
  EXPECT_FALSE(toggle_effects_view());

  // Click the toggle button, bubble is taken down.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(bubble_view());
}

TEST_F(BubbleViewTest, RegisterToggleEffect) {
  // Open up the bubble, no toggle effects present.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(toggle_effects_view());

  // Close the bubble.
  LeftClickOn(toggle_bubble_button());

  // Add one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());

  // Open up the bubble, toggle effects container view is present/visible.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(toggle_effects_view());
  EXPECT_TRUE(toggle_effects_view()->GetVisible());
}

TEST_F(BubbleViewTest, UnregisterToggleEffect) {
  // Add one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());

  // Open up the bubble, toggle effects are present/visible.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(toggle_effects_view());
  EXPECT_TRUE(toggle_effects_view()->GetVisible());

  // Take down the bubble.
  LeftClickOn(toggle_bubble_button());

  // Remove the toggle effect.
  controller()->effects_manager().UnregisterDelegate(office_bunny());

  // Open up the bubble again, no effects present.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(toggle_effects_view());
}

TEST_F(BubbleViewTest, ToggleButtonClicked) {
  // Add one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());

  // Click to open the bubble, toggle effect button is present/visible.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(toggle_effect_button());
  EXPECT_TRUE(toggle_effect_button()->GetVisible());

  // Toggle effect button has not yet been clicked.
  EXPECT_EQ(office_bunny()->num_activations_for_testing(), 0);

  // Click the toggle effect button, verify that the effect has been "activated"
  // once.
  LeftClickOn(toggle_effect_button());
  EXPECT_EQ(office_bunny()->num_activations_for_testing(), 1);
}

TEST_F(BubbleViewTest, RegisterSetValueEffect) {
  // Open up the bubble, no set-value effects present.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(toggle_effects_view());

  // Close the bubble.
  LeftClickOn(toggle_bubble_button());

  // Add one set-value effect.
  controller()->effects_manager().RegisterDelegate(shaggy_fur());

  // Open up the bubble, set-value effects container view is present/visible.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(set_value_effects_view());
  EXPECT_TRUE(set_value_effects_view()->GetVisible());
}

TEST_F(BubbleViewTest, UnregisterSetValueEffect) {
  // Add one set-value effect.
  controller()->effects_manager().RegisterDelegate(shaggy_fur());

  // Open up the bubble, set-value effects are present/visible.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(set_value_effects_view());
  EXPECT_TRUE(set_value_effects_view()->GetVisible());

  // Take down the bubble.
  LeftClickOn(toggle_bubble_button());

  // Remove the set-value effect.
  controller()->effects_manager().UnregisterDelegate(shaggy_fur());

  // Open up the bubble again, no effects present.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(set_value_effects_view());
}

TEST_F(BubbleViewTest, SetValueButtonClicked) {
  // Verify that the delegate hosts a single effect which has at least two
  // values.
  EXPECT_EQ(shaggy_fur()->GetNumEffects(), 1);
  EXPECT_GE(
      shaggy_fur()->GetEffectById(VcEffectId::kTestEffect)->GetNumStates(), 2);

  // Add one set-value effect.
  controller()->effects_manager().RegisterDelegate(shaggy_fur());

  // Click to open the bubble, effect value 0 button is present/visible.
  LeftClickOn(toggle_bubble_button());
  views::View* button = GetSetValueEffectButton(0);
  EXPECT_TRUE(button);
  EXPECT_TRUE(button->GetVisible());

  // Effect button for value 0 has not yet been clicked.
  EXPECT_EQ(shaggy_fur()->GetNumActivationsForTesting(0), 0);

  // Click the effect value 0 button, verify that the value has been "activated"
  // once.
  LeftClickOn(button);
  EXPECT_EQ(shaggy_fur()->GetNumActivationsForTesting(0), 1);

  // Now test another button, confirm that set-value effect button 1 is
  // present/visible.
  button = GetSetValueEffectButton(1);
  EXPECT_TRUE(button);
  EXPECT_TRUE(button->GetVisible());

  // Effect button for value 1 has not yet been clicked.
  EXPECT_EQ(shaggy_fur()->GetNumActivationsForTesting(1), 0);

  // Click the effect value 1 button, verify that value 1 has been "activated"
  // once, and confirm that value 0 has still only been activated once i.e. we
  // just activated value 1 and not value 0.
  LeftClickOn(button);
  EXPECT_EQ(shaggy_fur()->GetNumActivationsForTesting(1), 1);
  EXPECT_EQ(shaggy_fur()->GetNumActivationsForTesting(0), 1);
}

TEST_F(BubbleViewTest, ValidEffectState) {
  // Verify that the delegate hosts a single effect which has at least two
  // values.
  EXPECT_EQ(super_cuteness()->GetNumEffects(), 1);
  EXPECT_GE(
      super_cuteness()->GetEffectById(VcEffectId::kTestEffect)->GetNumStates(),
      2);

  // Add one set-value effect.
  controller()->effects_manager().RegisterDelegate(super_cuteness());

  // Effect will NOT return an invalid state.
  super_cuteness()->set_has_invalid_effect_state_for_testing(false);

  // Click to open the bubble, a single set-value effect view is
  // present/visible.
  LeftClickOn(toggle_bubble_button());
  views::View* effect_view = single_set_value_effect_view();
  EXPECT_TRUE(effect_view);
  EXPECT_TRUE(effect_view->GetVisible());
}

TEST_F(BubbleViewTest, InvalidEffectState) {
  // Verify that the delegate hosts a single effect which has at least two
  // values.
  EXPECT_EQ(super_cuteness()->GetNumEffects(), 1);
  EXPECT_GE(
      super_cuteness()->GetEffectById(VcEffectId::kTestEffect)->GetNumStates(),
      2);

  // Add one set-value effect.
  controller()->effects_manager().RegisterDelegate(super_cuteness());

  // Effect WILL return an invalid state.
  super_cuteness()->set_has_invalid_effect_state_for_testing(true);

  // Click to open the bubble, a single set-value effect view is NOT present.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(single_set_value_effect_view());
}

// The four `bool` params are as follows, if 'true':
//    0 - The test effects depend on the camera being enabled.
//    1 - The test effects depend on the microphone being enabled.
//    2 - The camera is enabled.
//    3 - The microphone is enabled.
//    4 - The camera has granted permission to running media app(s).
//    5 - The microphone has granted permission to running media app(s).
class ResourceDependencyTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, bool, bool, bool, bool, bool>> {
 public:
  ResourceDependencyTest() = default;
  ResourceDependencyTest(const ResourceDependencyTest&) = delete;
  ResourceDependencyTest& operator=(const ResourceDependencyTest&) = delete;
  ~ResourceDependencyTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVideoConference);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCameraEffectsSupportedByHardware);

    // Here we have to create the global instance of `CrasAudioHandler` before
    // `FakeVideoConferenceTrayController`, so we do it here and not do it in
    // `AshTestBase`.
    CrasAudioClient::InitializeFake();
    CrasAudioHandler::InitializeForTesting();

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    set_create_global_cras_audio_handler(false);
    AshTestBase::SetUp();

    // Make the video conference tray visible for testing.
    video_conference_tray()->SetVisiblePreferred(true);

    // The test is executed with each possible combination of these.
    has_camera_dependency_ = std::get<0>(GetParam());
    has_microphone_dependency_ = std::get<1>(GetParam());
    camera_enabled_ = std::get<2>(GetParam());
    microphone_enabled_ = std::get<3>(GetParam());
    has_camera_permission_ = std::get<4>(GetParam());
    has_microphone_permission_ = std::get<5>(GetParam());
  }

  void TearDown() override {
    AshTestBase::TearDown();
    toggle_effect_.reset();
    set_value_effect_.reset();
    controller_.reset();
    CrasAudioHandler::Shutdown();
    CrasAudioClient::Shutdown();
  }

  void CreateTestEffects(
      VcHostedEffect::ResourceDependencyFlags dependency_flags) {
    toggle_effect_ = std::make_unique<SquareCinnamonCereal>(dependency_flags);
    controller()->effects_manager().RegisterDelegate(toggle_effect_.get());
    set_value_effect_ = std::make_unique<SnackNationForever>(dependency_flags);
    controller()->effects_manager().RegisterDelegate(set_value_effect_.get());
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  IconButton* toggle_bubble_button() {
    return video_conference_tray()->toggle_bubble_button_;
  }

  views::View* bubble_view() {
    return video_conference_tray()->GetBubbleView();
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

  views::View* effects_view() {
    return bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kToggleEffectsView);
  }

  SquareCinnamonCereal* toggle_effect() { return toggle_effect_.get(); }

  SnackNationForever* set_value_effect() { return set_value_effect_.get(); }

  views::View* toggle_effect_container_view() {
    return bubble_view()->GetViewByID(kSquareCinnamonCerealViewId);
  }

  views::View* set_value_effect_container_view() {
    return bubble_view()->GetViewByID(kSnackNationForeverViewId);
  }

  bool has_camera_dependency() const { return has_camera_dependency_; }
  bool has_microphone_dependency() const { return has_microphone_dependency_; }
  bool camera_enabled() const { return camera_enabled_; }
  bool microphone_enabled() const { return microphone_enabled_; }
  bool has_camera_permission() const { return has_camera_permission_; }
  bool has_microphone_permission() const { return has_microphone_permission_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
  std::unique_ptr<SquareCinnamonCereal> toggle_effect_;
  std::unique_ptr<SnackNationForever> set_value_effect_;

  bool has_camera_dependency_ = false;
  bool has_microphone_dependency_ = false;
  bool camera_enabled_ = false;
  bool microphone_enabled_ = false;
  bool has_camera_permission_ = false;
  bool has_microphone_permission_ = false;
};

INSTANTIATE_TEST_SUITE_P(BubbleViewResourceDependency,
                         ResourceDependencyTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

TEST_P(ResourceDependencyTest, ResourceDependency) {
  // Initialize with no dependencies.
  VcHostedEffect::ResourceDependencyFlags flags =
      VcHostedEffect::ResourceDependency::kNone;

  // Set camera dependency.
  if (has_camera_dependency()) {
    flags |= VcHostedEffect::ResourceDependency::kCamera;
  }

  // Set microphone dependency.
  if (has_microphone_dependency()) {
    flags |= VcHostedEffect::ResourceDependency::kMicrophone;
  }

  // Instantiate/register the fake effects.
  CreateTestEffects(flags);

  // Camera is enabled.
  controller()->SetCameraMuted(!camera_enabled());

  // Microphone is enabled.
  controller()->SetMicrophoneMuted(!microphone_enabled());

  VideoConferenceMediaState state;
  state.has_camera_permission = has_camera_permission();
  state.has_microphone_permission = has_microphone_permission();
  controller()->UpdateWithMediaState(state);

  // Click to open the bubble, bubble is present/visible.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(bubble_view());
  EXPECT_TRUE(bubble_view()->GetVisible());

  // Effect container view is present/visible if its dependencies are
  // satisfied. A dependency on a resource is considered "satfisfied" if (1)
  // there is no dependency on the resource or (2) there is a dependency on the
  // resource, the resource is enabled, and there's at least one running media
  // app(s) has been granted permission to use the resource.
  const bool camera_satisfied =
      !has_camera_dependency() ||
      (has_camera_dependency() && has_camera_permission());
  const bool microphone_satisfied =
      !has_microphone_dependency() ||
      (has_microphone_dependency() && has_microphone_permission());

  if (camera_satisfied && microphone_satisfied) {
    EXPECT_TRUE(toggle_effect_container_view());
    EXPECT_TRUE(toggle_effect_container_view()->GetVisible());
    EXPECT_TRUE(set_value_effect_container_view());
    EXPECT_TRUE(set_value_effect_container_view()->GetVisible());
  } else {
    EXPECT_FALSE(toggle_effect_container_view());
    EXPECT_FALSE(set_value_effect_container_view());
  }
}

}  // namespace ash::video_conference
