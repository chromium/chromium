// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

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
#include "ash/system/video_conference/effects/fake_video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-shared.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "url/gurl.h"

namespace ash::video_conference {

namespace {

const char kMeetTestUrl[] = "https://meet.google.com/abc-xyz/ab-123";

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

class BubbleViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface</*IsVcDlcUiEnabled*/ bool> {
 public:
  BubbleViewPixelTest() = default;
  BubbleViewPixelTest(const BubbleViewPixelTest&) = delete;
  BubbleViewPixelTest& operator=(const BubbleViewPixelTest&) = delete;
  ~BubbleViewPixelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features{
        features::kFeatureManagementVideoConference};
    // TODO(b/334375880): Add a specific pixel test for the feature
    // VcBackgroundReplace.
    std::vector<base::test::FeatureRef> disabled_features{
        features::kVcBackgroundReplace};
    if (IsVcDlcUiEnabled()) {
      enabled_features.push_back(features::kVcDlcUi);
    } else {
      disabled_features.push_back(features::kVcDlcUi);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    if (IsVcDlcUiEnabled()) {
      DlcserviceClient::InitializeFake();
    }

    office_bunny_ =
        std::make_unique<fake_video_conference::OfficeBunnyEffect>();
    cat_ears_ = std::make_unique<fake_video_conference::CatEarsEffect>();
    long_text_effect_ = std::make_unique<
        fake_video_conference::FakeLongTextLabelToggleEffect>();
    shaggy_fur_ = std::make_unique<fake_video_conference::ShaggyFurEffect>();

    // Instantiates a fake controller (the real one is created in
    // `ChromeBrowserMainExtraPartsAsh::PreProfileInit()` which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();
    if (IsVcDlcUiEnabled()) {
      // When `VcDlcUi` is enabled we also need to use a fake effects manager,
      // since the toggle effects tiles in these tests all use the same
      // `VcEffectId::kTestEffect` id; the real effects manager would only be
      // able to return a unique tile UI controller for a given effect id, which
      // would result in all the test tiles being identical (same icon, title,
      // etc.).
      fake_effects_manager_ =
          std::make_unique<FakeVideoConferenceTrayEffectsManager>();
      controller_->SetEffectsManager(fake_effects_manager_.get());
    }

    AshTestBase::SetUp();

    // Make the video conference tray visible for testing.
    video_conference_tray()->SetVisiblePreferred(true);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    controller_.reset();
    if (IsVcDlcUiEnabled()) {
      fake_effects_manager_.reset();
    }
    shaggy_fur_.reset();
    long_text_effect_.reset();
    cat_ears_.reset();
    office_bunny_.reset();
    if (IsVcDlcUiEnabled()) {
      DlcserviceClient::Shutdown();
    }
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  bool IsVcDlcUiEnabled() { return GetParam(); }

  void ModifyDlcDownloadState(bool add_warning, std::u16string warning_label) {
    static_cast<video_conference::BubbleView*>(bubble_view())
        ->OnDLCDownloadStateInError(add_warning, warning_label);
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
  std::unique_ptr<FakeVideoConferenceTrayEffectsManager> fake_effects_manager_;
  std::unique_ptr<fake_video_conference::OfficeBunnyEffect> office_bunny_;
  std::unique_ptr<fake_video_conference::CatEarsEffect> cat_ears_;
  std::unique_ptr<fake_video_conference::FakeLongTextLabelToggleEffect>
      long_text_effect_;
  std::unique_ptr<fake_video_conference::ShaggyFurEffect> shaggy_fur_;
};

INSTANTIATE_TEST_SUITE_P(IsVcDlcUiEnabled,
                         BubbleViewPixelTest,
                         testing::Bool());

// Captures the basic bubble view with one media app, 2 toggle effects and 1 set
// value effects.
TEST_P(BubbleViewPixelTest, Basic) {
  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));

  // Add 2 toggle effects.
  controller()->GetEffectsManager().RegisterDelegate(office_bunny());
  controller()->GetEffectsManager().RegisterDelegate(long_text_effect());

  // Add one set-value effect.
  controller()->GetEffectsManager().RegisterDelegate(shaggy_fur());

  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_basic",
      /*revision_number=*/15, bubble_view()));
}

// Pixel test that tests toggled on/off and focused/not focused for the toggle
// effect button.
TEST_P(BubbleViewPixelTest, ToggleButton) {
  // Add one toggle effect.
  controller()->GetEffectsManager().RegisterDelegate(office_bunny());

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
      /*revision_number=*/13, toggle_effect_button_container));

  // Toggle the first button, the UI should change.
  LeftClickOn(first_toggle_effect_button);
  ASSERT_EQ(1, office_bunny()->num_activations_for_testing());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_no_focus_toggled",
      /*revision_number=*/10, toggle_effect_button_container));

  // Un-toggle the button, then keyboard focus it.
  LeftClickOn(first_toggle_effect_button);
  ASSERT_EQ(2, office_bunny()->num_activations_for_testing());
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  ASSERT_TRUE(first_toggle_effect_button->HasFocus());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_with_focus_not_toggled",
      /*revision_number=*/13, toggle_effect_button_container));

  // Re-toggle the button.
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  ASSERT_EQ(3, office_bunny()->num_activations_for_testing());
  ASSERT_TRUE(first_toggle_effect_button->HasFocus());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_with_focus_toggled",
      /*revision_number=*/12, toggle_effect_button_container));
}

// Pixel test that tests the expanded/collapsed state of the return to app panel
// when there's one and two running media app.
TEST_P(BubbleViewPixelTest, ReturnToApp) {
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
      /*revision_number=*/8, GetReturnToAppPanel()));

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
      /*revision_number=*/8, return_to_app_panel));

  // Click the summary row to expand the panel.
  auto* summary_row = static_cast<video_conference::ReturnToAppButton*>(
      GetReturnToAppContainer()->children().front());
  LeftClickOn(summary_row);
  ASSERT_TRUE(summary_row->expanded());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_tray_return_to_app_two_apps_expanded",
      /*revision_number=*/8, return_to_app_panel));
}

TEST_P(BubbleViewPixelTest, ReturnToAppLinux) {
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
      /*revision_number=*/9, video_conference_tray()->GetBubbleView()));

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
      /*revision_number=*/9, video_conference_tray()->GetBubbleView()));
}

TEST_P(BubbleViewPixelTest, OneToggleEffects) {
  // Add 1 toggle effects.
  controller()->GetEffectsManager().RegisterDelegate(long_text_effect());

  // Click to open the bubble, toggle effect button should be visible.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());
  ASSERT_TRUE(GetToggleEffectsView()->GetVisible());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_one_toggle_effect",
      /*revision_number=*/6, GetToggleEffectsView()));
}

TEST_P(BubbleViewPixelTest, TwoToggleEffects) {
  // Add 2 toggle effects.
  controller()->GetEffectsManager().RegisterDelegate(office_bunny());
  controller()->GetEffectsManager().RegisterDelegate(long_text_effect());

  // Click to open the bubble, toggle effect button should be visible.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());
  ASSERT_TRUE(GetToggleEffectsView()->GetVisible());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_two_toggle_effects",
      /*revision_number=*/6, GetToggleEffectsView()));
}

TEST_P(BubbleViewPixelTest, ThreeToggleEffects) {
  // Add 3 toggle effects.
  // To test multi-line label in the toggle button, we test with 3 strings with
  // different length: (1) small string that fits into 1 line ("Cat Ears"), (2)
  // relatively small string that is just a bit more than 1 line ("Office
  // Bunny"), and (3) long string that is definitely not fit into 1 line.
  controller()->GetEffectsManager().RegisterDelegate(office_bunny());
  controller()->GetEffectsManager().RegisterDelegate(cat_ears());
  controller()->GetEffectsManager().RegisterDelegate(long_text_effect());

  // Click to open the bubble, toggle effect button should be visible.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());
  ASSERT_TRUE(GetToggleEffectsView()->GetVisible());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_three_toggle_effects",
      /*revision_number=*/6, GetToggleEffectsView()));
}

TEST_P(BubbleViewPixelTest, DLCUIInErrorShowsWarningLabelSingleError) {
  if (!IsVcDlcUiEnabled()) {
    return;
  }
  // Create a toggle effect so the warning label is available.
  controller()->GetEffectsManager().RegisterDelegate(office_bunny());

  // Click to open the bubble, toggle effect button should be visible.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());
  ASSERT_TRUE(GetToggleEffectsView()->GetVisible());

  // Add an error, make sure the label shows up.
  ModifyDlcDownloadState(/*add_warning=*/true, u"test-feature-name1");

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "one-toggle-effects-view",
      /*revision_number=*/3, bubble_view()));

  // Add one set-value effect.
  controller()->GetEffectsManager().RegisterDelegate(shaggy_fur());

  // Hide and re-show the bubble so the set-value effect show sup.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_FALSE(bubble_view());

  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());

  ModifyDlcDownloadState(/*add_warning=*/true, u"test-feature-name1");

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "additional-set-value-view",
      /*revision_number=*/3, bubble_view()));
}

TEST_P(BubbleViewPixelTest, DLCUIInErrorShowsWarningLabelMaxErrors) {
  if (!IsVcDlcUiEnabled()) {
    return;
  }
  // Create a toggle effect so the warning label is available.
  controller()->GetEffectsManager().RegisterDelegate(office_bunny());

  // Click to open the bubble, toggle effect button should be visible.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());
  ASSERT_TRUE(GetToggleEffectsView()->GetVisible());

  // Add 2 errors (the max), make sure the label shows up.
  ModifyDlcDownloadState(/*add_warning=*/true, u"test-feature-name1");
  ModifyDlcDownloadState(/*add_warning=*/true, u"test-feature-name2");

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "one-toggle-effects-view",
      /*revision_number=*/3, bubble_view()));

  // Add one set-value effect.
  controller()->GetEffectsManager().RegisterDelegate(shaggy_fur());

  // Hide and re-show the bubble so the set-value effect show sup.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_FALSE(bubble_view());

  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());
  ASSERT_TRUE(bubble_view());

  ModifyDlcDownloadState(/*add_warning=*/true, u"test-feature-name1");
  ModifyDlcDownloadState(/*add_warning=*/true, u"test-feature-name2");

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "additional-set-value-view",
      /*revision_number=*/3, bubble_view()));
}

}  // namespace ash::video_conference
