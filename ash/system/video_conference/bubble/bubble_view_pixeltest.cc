// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/bubble/bubble_view.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/return_to_app_panel.h"
#include "ash/system/video_conference/bubble/toggle_effects_view.h"
#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-shared.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "url/gurl.h"

namespace ash::video_conference {

namespace {

const std::string kMeetTestUrl = "https://meet.google.com/abc-xyz/ab-123";

crosapi::mojom::VideoConferenceMediaAppInfoPtr CreateFakeMediaApp(
    bool is_capturing_camera,
    bool is_capturing_microphone,
    bool is_capturing_screen,
    const std::u16string& title,
    std::string url,
    const crosapi::mojom::VideoConferenceAppType app_type =
        crosapi::mojom::VideoConferenceAppType::kChromeTab,
    const base::UnguessableToken& id = base::UnguessableToken::Create()) {
  return crosapi::mojom::VideoConferenceMediaAppInfo::New(
      id,
      /*last_activity_time=*/base::Time::Now(), is_capturing_camera,
      is_capturing_microphone, is_capturing_screen, title,
      /*url=*/GURL(url), app_type);
}

}  // namespace

class BubbleViewPixelTest : public AshTestBase {
 public:
  BubbleViewPixelTest() = default;
  BubbleViewPixelTest(const BubbleViewPixelTest&) = delete;
  BubbleViewPixelTest& operator=(const BubbleViewPixelTest&) = delete;
  ~BubbleViewPixelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kVideoConference,
         features::kCameraEffectsSupportedByHardware,
         chromeos::features::kJelly},
        {});

    // Instantiates a fake controller (the real one is created in
    // `ChromeBrowserMainExtraPartsAsh::PreProfileInit()` which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    office_bunny_ =
        std::make_unique<fake_video_conference::OfficeBunnyEffect>();
    cat_ears_ = std::make_unique<fake_video_conference::CatEarsEffect>();
    long_text_effect_ = std::make_unique<
        fake_video_conference::FakeLongTextLabelToggleEffect>();
    shaggy_fur_ = std::make_unique<fake_video_conference::ShaggyFurEffect>();

    AshTestBase::SetUp();

    // Make the video conference tray visible for testing.
    video_conference_tray()->SetVisiblePreferred(true);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    shaggy_fur_.reset();
    long_text_effect_.reset();
    cat_ears_.reset();
    office_bunny_.reset();
    controller_.reset();
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  views::View* bubble_view() {
    return video_conference_tray()->GetBubbleView();
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

  // Each toggle button in the bubble view has this view ID, this just gets the
  // first one in the view tree.
  views::Button* GetFirstToggleEffectButton() {
    return static_cast<views::Button*>(bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kToggleEffectsButton));
  }

  views::View* GetToggleEffectsView() {
    return bubble_view()->GetViewByID(BubbleViewID::kToggleEffectsView);
  }

  // Get the `ReturnToAppPanel` from the test `StatusAreaWidget`.
  video_conference::ReturnToAppPanel* GetReturnToAppPanel() {
    return static_cast<video_conference::ReturnToAppPanel*>(
        video_conference_tray()->GetBubbleView()->GetViewByID(
            video_conference::BubbleViewID::kReturnToApp));
  }

  video_conference::ReturnToAppPanel::ReturnToAppContainer*
  GetReturnToAppContainer() {
    return GetReturnToAppPanel()->container_view_;
  }

  // Make the tray and buttons visible by setting `VideoConferenceMediaState`,
  // and return the state so it can be modified.
  VideoConferenceMediaState SetTrayAndButtonsVisible() {
    VideoConferenceMediaState state;
    state.has_media_app = true;
    state.has_camera_permission = true;
    state.has_microphone_permission = true;
    state.is_capturing_screen = true;
    controller()->UpdateWithMediaState(state);
    return state;
  }

  fake_video_conference::OfficeBunnyEffect* office_bunny() {
    return office_bunny_.get();
  }

  fake_video_conference::CatEarsEffect* cat_ears() { return cat_ears_.get(); }

  fake_video_conference::FakeLongTextLabelToggleEffect* long_text_effect() {
    return long_text_effect_.get();
  }

  fake_video_conference::ShaggyFurEffect* shaggy_fur() {
    return shaggy_fur_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
  std::unique_ptr<fake_video_conference::OfficeBunnyEffect> office_bunny_;
  std::unique_ptr<fake_video_conference::CatEarsEffect> cat_ears_;
  std::unique_ptr<fake_video_conference::FakeLongTextLabelToggleEffect>
      long_text_effect_;
  std::unique_ptr<fake_video_conference::ShaggyFurEffect> shaggy_fur_;
};

// Captures the basic bubble view with one media app, 2 toggle effects and 1 set
// value effects.
TEST_F(BubbleViewPixelTest, Basic) {
  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));

  // Add 2 toggle effects.
  controller()->effects_manager().RegisterDelegate(office_bunny());
  controller()->effects_manager().RegisterDelegate(long_text_effect());

  // Add one set-value effect.
  controller()->effects_manager().RegisterDelegate(shaggy_fur());

  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_basic",
      /*revision_number=*/8, bubble_view()));
}

// Pixel test that tests toggled on/off and focused/not focused for the toggle
// effect button.
TEST_F(BubbleViewPixelTest, ToggleButton) {
  // Add one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());

  // Click to open the bubble, toggle effect button should be visible.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());

  ASSERT_TRUE(bubble_view());
  auto* first_toggle_effect_button = GetFirstToggleEffectButton();
  ASSERT_TRUE(first_toggle_effect_button);

  // The bounds paint slightly outside of `first_toggle_effect_button`'s bounds,
  // so grab the scroll view's contents view. This is sterile for this pixel
  // test because the test effect (office bunny) only has a single toggle with
  // no sliders.
  auto* toggle_effect_button_container = GetToggleEffectsView()->parent();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_no_focus_not_toggled",
      /*revision_number=*/11, toggle_effect_button_container));

  // Toggle the first button, the UI should change.
  LeftClickOn(first_toggle_effect_button);
  ASSERT_EQ(1, office_bunny()->num_activations_for_testing());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_no_focus_toggled",
      /*revision_number=*/8, toggle_effect_button_container));

  // Un-toggle the button, then keyboard focus it.
  LeftClickOn(first_toggle_effect_button);
  ASSERT_EQ(2, office_bunny()->num_activations_for_testing());
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  ASSERT_TRUE(first_toggle_effect_button->HasFocus());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_with_focus_not_toggled",
      /*revision_number=*/11, toggle_effect_button_container));

  // Re-toggle the button.
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  ASSERT_EQ(3, office_bunny()->num_activations_for_testing());
  ASSERT_TRUE(first_toggle_effect_button->HasFocus());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_with_focus_toggled",
      /*revision_number=*/10, toggle_effect_button_container));
}

// Pixel test that tests the expanded/collapsed state of the return to app panel
// when there's one and two running media app.
TEST_F(BubbleViewPixelTest, ReturnToApp) {
  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));

  SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());

  auto* toggle_bubble_button = video_conference_tray()->toggle_bubble_button();
  LeftClickOn(toggle_bubble_button);
  ASSERT_TRUE(video_conference_tray()->GetBubbleView());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_return_to_app_one_app",
      /*revision_number=*/3, GetReturnToAppPanel()));

  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));

  // Double click to reset the bubble to show the newly added media app.
  LeftClickOn(toggle_bubble_button);
  LeftClickOn(toggle_bubble_button);
  ASSERT_TRUE(video_conference_tray()->GetBubbleView());

  auto* return_to_app_panel = GetReturnToAppPanel();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_return_to_app_two_apps_collapsed",
      /*revision_number=*/3, return_to_app_panel));

  // Click the summary row to expand the panel.
  auto* summary_row = static_cast<video_conference::ReturnToAppButton*>(
      GetReturnToAppContainer()->children().front());
  LeftClickOn(summary_row);
  ASSERT_TRUE(summary_row->expanded());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_return_to_app_two_apps_expanded",
      /*revision_number=*/3, return_to_app_panel));
}

TEST_F(BubbleViewPixelTest, ReturnToAppLinux) {
  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Linux",
      /*url=*/"",
      /*app_type=*/crosapi::mojom::VideoConferenceAppType::kCrostiniVm));

  SetTrayAndButtonsVisible();
  ASSERT_TRUE(video_conference_tray()->GetVisible());

  auto* toggle_bubble_button = video_conference_tray()->toggle_bubble_button();
  LeftClickOn(toggle_bubble_button);
  ASSERT_TRUE(video_conference_tray()->GetBubbleView());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_linux_bubble_one_app",
      /*revision_number=*/8, video_conference_tray()->GetBubbleView()));

  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/false, /*title=*/u"Parallels",
      /*url=*/"",
      /*app_type=*/crosapi::mojom::VideoConferenceAppType::kPluginVm));

  // Double click to reset the bubble to show the newly added media app.
  LeftClickOn(toggle_bubble_button);
  LeftClickOn(toggle_bubble_button);
  ASSERT_TRUE(video_conference_tray()->GetBubbleView());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_linux_bubble_two_app",
      /*revision_number=*/8, video_conference_tray()->GetBubbleView()));
}

TEST_F(BubbleViewPixelTest, OneToggleEffects) {
  // Add 1 toggle effects.
  controller()->effects_manager().RegisterDelegate(long_text_effect());

  // Click to open the bubble, toggle effect button should be visible.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());
  ASSERT_TRUE(GetToggleEffectsView()->GetVisible());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_one_toggle_effect",
      /*revision_number=*/5, GetToggleEffectsView()));
}

TEST_F(BubbleViewPixelTest, TwoToggleEffects) {
  // Add 2 toggle effects.
  controller()->effects_manager().RegisterDelegate(office_bunny());
  controller()->effects_manager().RegisterDelegate(long_text_effect());

  // Click to open the bubble, toggle effect button should be visible.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());
  ASSERT_TRUE(GetToggleEffectsView()->GetVisible());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_two_toggle_effects",
      /*revision_number=*/5, GetToggleEffectsView()));
}

TEST_F(BubbleViewPixelTest, ThreeToggleEffects) {
  // Add 3 toggle effects.
  // To test multi-line label in the toggle button, we test with 3 strings with
  // different length: (1) small string that fits into 1 line ("Cat Ears"), (2)
  // relatively small string that is just a bit more than 1 line ("Office
  // Bunny"), and (3) long string that is definitely not fit into 1 line.
  controller()->effects_manager().RegisterDelegate(office_bunny());
  controller()->effects_manager().RegisterDelegate(cat_ears());
  controller()->effects_manager().RegisterDelegate(long_text_effect());

  // Click to open the bubble, toggle effect button should be visible.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());
  ASSERT_TRUE(GetToggleEffectsView()->GetVisible());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_three_toggle_effects",
      /*revision_number=*/5, GetToggleEffectsView()));
}

}  // namespace ash::video_conference
