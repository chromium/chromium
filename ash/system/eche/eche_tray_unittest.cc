// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_tray.h"

#include <memory>
#include <string>

#include "ash/bubble/bubble_constants.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/image/image.h"

namespace ash {

namespace {

bool is_web_content_unloaded_ = false;
size_t num_web_content_go_back_calls_ = 0;

void UnloadWebContent() {
  is_web_content_unloaded_ = true;
}

void ResetUnloadWebContent() {
  is_web_content_unloaded_ = false;
}

void WebContentGoBack() {
  ++num_web_content_go_back_calls_;
}

void ResetWebContentGoBack() {
  num_web_content_go_back_calls_ = 0;
}

SkBitmap TestBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(30, 30);
  return bitmap;
}

gfx::Image CreateTestImage() {
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(TestBitmap());
  image_skia.MakeThreadSafe();
  return gfx::Image(image_skia);
}

}  // namespace

class EcheTrayTest : public AshTestBase {
 public:
  EcheTrayTest() = default;

  EcheTrayTest(const EcheTrayTest&) = delete;
  EcheTrayTest& operator=(const EcheTrayTest&) = delete;

  ~EcheTrayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA},
        /*disabled_features=*/{});

    DCHECK(test_web_view_factory_.get());

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);

    AshTestBase::SetUp();

    eche_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();
    phone_hub_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->phone_hub_tray();

    display::test::DisplayManagerTestApi(display_manager())
        .SetFirstDisplayAsInternalDisplay();

    toast_manager_ = Shell::Get()->toast_manager();
  }

  // Performs a tap on the eche tray button.
  void PerformTap() {
    GetEventGenerator()->GestureTapAt(
        eche_tray_->GetBoundsInScreen().CenterPoint());
  }

  void ClickButton(views::Button* button) {
    GetEventGenerator()->GestureTapAt(
        button->GetBoundsInScreen().CenterPoint());
  }

  EcheTray* eche_tray() { return eche_tray_; }
  PhoneHubTray* phone_hub_tray() { return phone_hub_tray_; }
  ToastManagerImpl* toast_manager() { return toast_manager_; }

 private:
  EcheTray* eche_tray_ = nullptr;  // Not owned
  PhoneHubTray* phone_hub_tray_ = nullptr;  // Not owned
  base::test::ScopedFeatureList feature_list_;
  ToastManagerImpl* toast_manager_ = nullptr;

  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

// Verify the Eche tray button exists and but is not visible initially.
TEST_F(EcheTrayTest, PaletteTrayIsInvisible) {
  ASSERT_TRUE(eche_tray());
  EXPECT_FALSE(eche_tray()->GetVisible());
}

// Verify taps on the eche tray button results in expected behaviour.
// It also sets the url and calls `LoadBubble`.
TEST_F(EcheTrayTest, EcheTrayShowBubbleAndTapTwice) {
  // Verify the eche tray button is not active, and the eche tray bubble
  // is not shown initially.
  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_FALSE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_FALSE(eche_tray()->GetVisible());

  eche_tray()->SetVisiblePreferred(true);
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_TRUE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_TRUE(eche_tray()
                  ->get_web_view_for_test()
                  ->GetInitiallyFocusedView()
                  ->HasFocus());

  // Verify that by tapping the eche tray button, the button will become
  // inactive and the eche tray bubble will be closed.
  PerformTap();
  // Wait for the tray bubble widget to close.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_FALSE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_TRUE(eche_tray()->GetVisible());

  // Verify that tapping again will show the bubble.
  PerformTap();
  // Wait for the tray bubble widget to open.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_TRUE(eche_tray()->GetVisible());
}

TEST_F(EcheTrayTest, EcheTrayIconResize) {
  eche_tray()->SetVisiblePreferred(true);
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  int image_width = phone_hub_tray()
                        ->eche_icon_view()
                        ->GetImage(views::ImageButton::STATE_NORMAL)
                        .width();

  eche_tray()->ResizeIcon(2);

  int new_image_width = phone_hub_tray()
                            ->eche_icon_view()
                            ->GetImage(views::ImageButton::STATE_NORMAL)
                            .width();

  EXPECT_EQ(image_width, new_image_width + 2);
}

TEST_F(EcheTrayTest, OnAnyBubbleVisibilityChanged) {
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  // When any other bubble is shown we need to hide Eche.
  eche_tray()->OnAnyBubbleVisibilityChanged(
      reinterpret_cast<views::Widget*>(12345L), true);

  EXPECT_FALSE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_FALSE(is_web_content_unloaded_);
}

// OnAnyBubbleVisibilityChanged() is called on the current bubble and hence
// should be ignored.
TEST_F(EcheTrayTest, OnAnyBubbleVisibilityChanged_SameWidget) {
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  eche_tray()->OnAnyBubbleVisibilityChanged(eche_tray()->GetBubbleWidget(),
                                            true);

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
}

// OnAnyBubbleVisibilityChanged() is called on some other bubble but the
// visible parameter is false, hence we should not do anything.
TEST_F(EcheTrayTest, OnAnyBubbleVisibilityChanged_NonVisible) {
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  eche_tray()->OnAnyBubbleVisibilityChanged(
      reinterpret_cast<views::Widget*>(12345L), false);

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
}

TEST_F(EcheTrayTest, EcheTrayCreatesBubbleButHideFirst) {
  // Verify the eche tray button is not active, and the eche tray bubble
  // is not shown initially.
  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_FALSE(eche_tray()->get_bubble_wrapper_for_test());

  // Allow us to create the bubble but it is not visible until we need this
  // bubble to show up.
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");

  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_FALSE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_TRUE(phone_hub_tray()->eche_loading_indicator()->GetAnimating());

  // Request this bubble to show up.
  eche_tray()->ShowBubble();
  // Wait for the tray bubble widget to open.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_FALSE(phone_hub_tray()->eche_loading_indicator()->GetAnimating());
}

TEST_F(EcheTrayTest, EcheTrayCreatesBubbleButStreamStatusChanged) {
  // Verify the eche tray button is not active, and the eche tray bubble
  // is not shown initially.
  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_FALSE(eche_tray()->get_bubble_wrapper_for_test());

  // Allow us to create the bubble but it is not visible until we need this
  // bubble to show up.
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");

  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_FALSE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  // When the streaming status changes, the bubble should show up.
  eche_tray()->OnStreamStatusChanged(
      eche_app::mojom::StreamStatus::kStreamStatusStarted);
  // Wait for the tray bubble widget to open.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  // Change the streaming status, the bubble should be closed.
  eche_tray()->OnStreamStatusChanged(
      eche_app::mojom::StreamStatus::kStreamStatusStopped);
  // Wait for the tray bubble widget to close.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_FALSE(eche_tray()->GetVisible());
}

TEST_F(EcheTrayTest, EcheTrayMinimizeButtonClicked) {
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  ClickButton(eche_tray()->GetMinimizeButtonForTesting());

  EXPECT_FALSE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_FALSE(is_web_content_unloaded_);
}

TEST_F(EcheTrayTest, EcheTrayCloseButtonClicked) {
  ResetUnloadWebContent();
  eche_tray()->SetGracefulCloseCallback(base::BindOnce(&UnloadWebContent));
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  ClickButton(eche_tray()->GetCloseButtonForTesting());

  EXPECT_TRUE(is_web_content_unloaded_);
}

TEST_F(EcheTrayTest, EcheTrayBackButtonClicked) {
  ResetWebContentGoBack();
  eche_tray()->SetGracefulGoBackCallback(
      base::BindRepeating(&WebContentGoBack));
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  ClickButton(eche_tray()->GetArrowBackButtonForTesting());

  EXPECT_EQ(1u, num_web_content_go_back_calls_);

  ClickButton(eche_tray()->GetArrowBackButtonForTesting());

  EXPECT_EQ(2u, num_web_content_go_back_calls_);
}

TEST_F(EcheTrayTest, AcceleratorKeyHandled_Minimize) {
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  // Press a random key that is NOT supposed to minimize Eche.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_1, ui::EF_ALT_DOWN);

  // Make sure it is still there.
  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  // Now press the alt+- that closes the bubble.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_OEM_MINUS,
                                ui::EF_ALT_DOWN);

  // Check to see if the bubble is closed.
  EXPECT_FALSE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_FALSE(is_web_content_unloaded_);
}

TEST_F(EcheTrayTest, AcceleratorKeyHandled_Ctrl_W) {
  ResetUnloadWebContent();
  eche_tray()->SetGracefulCloseCallback(base::BindOnce(&UnloadWebContent));
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  // Now press the ctrl+w that closes the bubble.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_W, ui::EF_CONTROL_DOWN);

  // Check to see if the bubble is closed and purged.
  EXPECT_TRUE(is_web_content_unloaded_);
}

TEST_F(EcheTrayTest, AcceleratorKeyHandled_Ctrl_C) {
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_FALSE(toast_manager()->IsRunning(
      "eche_tray_toast_ids.copy_paste_not_implemented"));

  // Now press the ctrl+w that closes the bubble.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_C, ui::EF_CONTROL_DOWN);

  // Check to see if a toast is shown
  EXPECT_TRUE(toast_manager()->IsRunning(
      "eche_tray_toast_ids.copy_paste_not_implemented"));
}

TEST_F(EcheTrayTest, AcceleratorKeyHandled_Ctrl_V) {
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_FALSE(toast_manager()->IsRunning(
      "eche_tray_toast_ids.copy_paste_not_implemented"));

  // Now press the ctrl+w that closes the bubble.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_V, ui::EF_CONTROL_DOWN);

  // Check to see if a toast is shown
  EXPECT_TRUE(toast_manager()->IsRunning(
      "eche_tray_toast_ids.copy_paste_not_implemented"));
}

TEST_F(EcheTrayTest, AcceleratorKeyHandled_Ctrl_X) {
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_FALSE(toast_manager()->IsRunning(
      "eche_tray_toast_ids.copy_paste_not_implemented"));

  // Now press the ctrl+w that closes the bubble.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_X, ui::EF_CONTROL_DOWN);

  // Check to see if a toast is shown
  EXPECT_TRUE(toast_manager()->IsRunning(
      "eche_tray_toast_ids.copy_paste_not_implemented"));
}

TEST_F(EcheTrayTest, AcceleratorKeyHandled_BROWSER_BACK_KEY) {
  ResetWebContentGoBack();
  eche_tray()->SetGracefulGoBackCallback(
      base::BindRepeating(&WebContentGoBack));
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_BROWSER_BACK, 0);

  EXPECT_EQ(1u, num_web_content_go_back_calls_);
}

TEST_F(EcheTrayTest, AcceleratorKeyHandled_Esc) {
  ResetUnloadWebContent();
  eche_tray()->SetGracefulCloseCallback(base::BindOnce(&UnloadWebContent));
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  // Now press the Esc that closes the bubble.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);

  // Check to see if the bubble is closed and purged.
  EXPECT_TRUE(is_web_content_unloaded_);
}

TEST_F(EcheTrayTest, EcheTrayOnDisplayConfigurationChanged) {
  UpdateDisplay("800x600");
  gfx::Size expected_eche_size = eche_tray()->CalculateSizeForEche();
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_EQ(expected_eche_size.width(),
            eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->width());
  EXPECT_EQ(
      expected_eche_size.height(),
      eche_tray()->get_web_view_for_test()->height() + kBubbleMenuPadding * 2);

  UpdateDisplay("1024x786");
  expected_eche_size = eche_tray()->CalculateSizeForEche();

  eche_tray()->OnDisplayConfigurationChanged();

  EXPECT_EQ(expected_eche_size.width(),
            eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->width());
  EXPECT_EQ(
      expected_eche_size.height(),
      eche_tray()->get_web_view_for_test()->height() + kBubbleMenuPadding * 2);
}

TEST_F(EcheTrayTest, EcheTrayKeyboardShowHideUpdateBubbleBounds) {
  gfx::Size expected_eche_size = eche_tray()->CalculateSizeForEche();
  eche_tray()->LoadBubble(GURL("http://google.com"), CreateTestImage(),
                          u"app 1", u"your phone");
  eche_tray()->ShowBubble();

  EXPECT_EQ(expected_eche_size.width(),
            eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->width());
  EXPECT_EQ(
      expected_eche_size.height(),
      eche_tray()->get_web_view_for_test()->height() + kBubbleMenuPadding * 2);

  // Place a keyboard window.
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(/*lock=*/true);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  EXPECT_EQ(expected_eche_size.width(),
            eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->width());

  // Hide the keyboard
  keyboard_controller->HideKeyboardByUser();

  EXPECT_EQ(expected_eche_size.width(),
            eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->width());
  EXPECT_EQ(
      expected_eche_size.height(),
      eche_tray()->get_web_view_for_test()->height() + kBubbleMenuPadding * 2);
}

}  // namespace ash
