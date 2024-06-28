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
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

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

gfx::Image CreateTestImage() {
  gfx::ImageSkia image_skia = gfx::test::CreateImageSkia(/*size=*/30);
  image_skia.MakeThreadSafe();
  return gfx::Image(image_skia);
}

}  // namespace

using ConnectionStatus = eche_app::mojom::ConnectionStatus;

class FakeConnectionStatusObserver
    : public eche_app::EcheConnectionStatusHandler::Observer {
 public:
  FakeConnectionStatusObserver() = default;
  ~FakeConnectionStatusObserver() override = default;

  size_t num_connection_status_for_ui_changed_calls() const {
    return num_connection_status_for_ui_changed_calls_;
  }

  ConnectionStatus last_connection_changed_for_ui_status() const {
    return last_connection_changed_for_ui_status_;
  }

  // eche_app::EcheConnectionStatusObserver::Observer:
  void OnConnectionStatusForUiChanged(
      ConnectionStatus connection_status) override {
    if (last_connection_changed_for_ui_status_ == connection_status) {
      return;
    }
    ++num_connection_status_for_ui_changed_calls_;
    last_connection_changed_for_ui_status_ = connection_status;
  }

 private:
  size_t num_connection_status_for_ui_changed_calls_ = 0;
  ConnectionStatus last_connection_changed_for_ui_status_ =
      ConnectionStatus::kConnectionStatusDisconnected;
};

class EcheTrayTest : public AshTestBase {
 public:
  EcheTrayTest() = default;

  EcheTrayTest(const EcheTrayTest&) = delete;
  EcheTrayTest& operator=(const EcheTrayTest&) = delete;

  ~EcheTrayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA,
                              features::kEcheNetworkConnectionState},
        /*disabled_features=*/{});

    DCHECK(test_web_view_factory_.get());

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);

    AshTestBase::SetUp();

    eche_connection_status_handler_ =
        std::make_unique<eche_app::EcheConnectionStatusHandler>();
    eche_connection_status_handler_->AddObserver(
        &fake_connection_status_observer_);
    eche_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();
    eche_tray_->SetEcheConnectionStatusHandler(
        eche_connection_status_handler_.get());
    phone_hub_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->phone_hub_tray();

    display::test::DisplayManagerTestApi(display_manager())
        .SetFirstDisplayAsInternalDisplay();
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

  size_t GetNumConnectionStatusForUiChangedCalls() {
    return fake_connection_status_observer_
        .num_connection_status_for_ui_changed_calls();
  }

  ConnectionStatus GetLastConnectionChangedForUiStatus() {
    return fake_connection_status_observer_
        .last_connection_changed_for_ui_status();
  }

  EcheTray* eche_tray() { return eche_tray_; }
  PhoneHubTray* phone_hub_tray() { return phone_hub_tray_; }

  base::test::ScopedFeatureList feature_list_;

 private:
  FakeConnectionStatusObserver fake_connection_status_observer_;
  raw_ptr<EcheTray, DanglingUntriaged> eche_tray_ = nullptr;  // Not owned
  raw_ptr<PhoneHubTray, DanglingUntriaged> phone_hub_tray_ =
      nullptr;  // Not owned

  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
  std::unique_ptr<eche_app::EcheConnectionStatusHandler>
      eche_connection_status_handler_;
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
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
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
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
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

TEST_F(EcheTrayTest, OnStatusAreaAnchoredBubbleVisibilityChanged) {
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray()->ShowBubble();

  auto* bubble_view = eche_tray()->get_bubble_wrapper_for_test()->bubble_view();

  EXPECT_TRUE(bubble_view->GetVisible());

  // When any other bubble is shown we need to hide Eche.
  auto test_bubble_widget = std::make_unique<TrayBubbleView>(
      CreateInitParamsForTrayBubble(phone_hub_tray()));
  eche_tray()->OnStatusAreaAnchoredBubbleVisibilityChanged(
      test_bubble_widget.get(), true);

  EXPECT_FALSE(bubble_view->GetVisible());
  EXPECT_FALSE(is_web_content_unloaded_);
}

// OnStatusAreaAnchoredBubbleVisibilityChanged() is called on the current bubble
// and hence should be ignored.
TEST_F(EcheTrayTest, OnStatusAreaAnchoredBubbleVisibilityChanged_SameWidget) {
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray()->ShowBubble();

  auto* bubble_view = eche_tray()->get_bubble_wrapper_for_test()->bubble_view();

  EXPECT_TRUE(bubble_view->GetVisible());

  eche_tray()->OnStatusAreaAnchoredBubbleVisibilityChanged(bubble_view, true);

  EXPECT_TRUE(bubble_view->GetVisible());
}

// OnStatusAreaAnchoredBubbleVisibilityChanged() is called on some other bubble
// but the visible parameter is false, hence we should not do anything.
TEST_F(EcheTrayTest, OnStatusAreaAnchoredBubbleVisibilityChanged_NonVisible) {
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray()->ShowBubble();

  auto* bubble_view = eche_tray()->get_bubble_wrapper_for_test()->bubble_view();

  EXPECT_TRUE(bubble_view->GetVisible());

  auto test_bubble_widget = std::make_unique<TrayBubbleView>(
      CreateInitParamsForTrayBubble(phone_hub_tray()));
  eche_tray()->OnStatusAreaAnchoredBubbleVisibilityChanged(
      test_bubble_widget.get(), false);

  EXPECT_TRUE(bubble_view->GetVisible());
}

TEST_F(EcheTrayTest, EcheTrayCreatesBubbleButHideFirst) {
  // Verify the eche tray button is not active, and the eche tray bubble
  // is not shown initially.
  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_FALSE(eche_tray()->get_bubble_wrapper_for_test());

  // Allow us to create the bubble but it is not visible until we need this
  // bubble to show up.
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);

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
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);

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
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
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
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray()->ShowBubble();

  ClickButton(eche_tray()->GetCloseButtonForTesting());

  EXPECT_TRUE(is_web_content_unloaded_);
}

TEST_F(EcheTrayTest, EcheTrayBackButtonClicked) {
  ResetWebContentGoBack();
  eche_tray()->SetGracefulGoBackCallback(
      base::BindRepeating(&WebContentGoBack));
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray()->ShowBubble();

  ClickButton(eche_tray()->GetArrowBackButtonForTesting());

  EXPECT_EQ(1u, num_web_content_go_back_calls_);

  ClickButton(eche_tray()->GetArrowBackButtonForTesting());

  EXPECT_EQ(2u, num_web_content_go_back_calls_);
}

TEST_F(EcheTrayTest, AcceleratorKeyHandled_Minimize) {
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
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
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray()->ShowBubble();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  // Now press the ctrl+w that closes the bubble.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_W, ui::EF_CONTROL_DOWN);

  // Check to see if the bubble is closed and purged.
  EXPECT_TRUE(is_web_content_unloaded_);
}

TEST_F(EcheTrayTest, AcceleratorKeyHandled_BROWSER_BACK_KEY) {
  ResetWebContentGoBack();
  eche_tray()->SetGracefulGoBackCallback(
      base::BindRepeating(&WebContentGoBack));
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray()->ShowBubble();

  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_BROWSER_BACK, 0);

  EXPECT_EQ(1u, num_web_content_go_back_calls_);
}

TEST_F(EcheTrayTest, AcceleratorKeyHandled_Esc) {
  ResetUnloadWebContent();
  eche_tray()->SetGracefulCloseCallback(base::BindOnce(&UnloadWebContent));
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
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
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray()->ShowBubble();

  EXPECT_EQ(expected_eche_size.width(),
            eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->width());
  EXPECT_EQ(
      expected_eche_size.height(),
      eche_tray()->get_web_view_for_test()->height() + kBubbleMenuPadding * 2);

  UpdateDisplay("1024x786");
  expected_eche_size = eche_tray()->CalculateSizeForEche();

  eche_tray()->OnDidApplyDisplayChanges();

  EXPECT_EQ(expected_eche_size.width(),
            eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->width());
  EXPECT_EQ(
      expected_eche_size.height(),
      eche_tray()->get_web_view_for_test()->height() + kBubbleMenuPadding * 2);
}

TEST_F(EcheTrayTest, EcheTrayKeyboardShowHideUpdateBubbleBounds) {
  gfx::Size expected_eche_size = eche_tray()->CalculateSizeForEche();
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray()->ShowBubble();

  EXPECT_EQ(expected_eche_size.width(),
            eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->width());
  EXPECT_EQ(
      expected_eche_size.height(),
      eche_tray()->get_web_view_for_test()->height() + kBubbleMenuPadding * 2);

  // Place a keyboard window.
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(/*lock=*/true);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

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

TEST_F(EcheTrayTest, EcheTrayOnStreamOrientationChanged) {
  gfx::Size expected_eche_size = eche_tray()->CalculateSizeForEche();
  eche_tray()->LoadBubble(
      GURL("http://google.com"), CreateTestImage(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray()->ShowBubble();

  EXPECT_EQ(eche_tray()->get_is_landscape_for_test(), false);
  EXPECT_EQ(expected_eche_size.width(),
            eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->width());
  EXPECT_EQ(
      expected_eche_size.height(),
      eche_tray()->get_web_view_for_test()->height() + kBubbleMenuPadding * 2);

  // Orientation should stay the same
  eche_tray()->OnStreamOrientationChanged(false);
  EXPECT_EQ(eche_tray()->get_is_landscape_for_test(), false);

  expected_eche_size = eche_tray()->CalculateSizeForEche();

  EXPECT_EQ(expected_eche_size.width(),
            eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->width());
  EXPECT_EQ(
      expected_eche_size.height(),
      eche_tray()->get_web_view_for_test()->height() + kBubbleMenuPadding * 2);

  // Change orientation
  eche_tray()->OnStreamOrientationChanged(true);
  EXPECT_EQ(eche_tray()->get_is_landscape_for_test(), true);

  expected_eche_size = eche_tray()->CalculateSizeForEche();
  EXPECT_EQ(
      expected_eche_size.width(),
      eche_tray()->get_web_view_for_test()->width() + kBubbleMenuPadding * 2);
  EXPECT_EQ(
      expected_eche_size.height(),
      eche_tray()->get_web_view_for_test()->height() + kBubbleMenuPadding * 2);
}

TEST_F(EcheTrayTest, OnRequestBackgroundConnectionAttempt) {
  ResetUnloadWebContent();

  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_FALSE(eche_tray()->get_initializer_webview_for_test());

  eche_tray()->OnRequestBackgroundConnectionAttempt();

  EXPECT_TRUE(eche_tray()->get_initializer_webview_for_test());
  EXPECT_FALSE(eche_tray()->is_active());
}

TEST_F(EcheTrayTest, OnRequestBackgroundConnectionAttemptFlagDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA},
      /*disabled_features=*/{features::kEcheNetworkConnectionState});

  ResetUnloadWebContent();

  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_FALSE(eche_tray()->get_initializer_webview_for_test());

  eche_tray()->OnRequestBackgroundConnectionAttempt();

  EXPECT_FALSE(eche_tray()->get_initializer_webview_for_test());
  EXPECT_FALSE(eche_tray()->is_active());
}

TEST_F(EcheTrayTest, OnConnectionStatusChanged) {
  EXPECT_EQ(GetLastConnectionChangedForUiStatus(),
            ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);

  eche_tray()->OnRequestBackgroundConnectionAttempt();

  eche_tray()->OnConnectionStatusChanged(
      ConnectionStatus::kConnectionStatusConnecting);

  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);
  EXPECT_TRUE(eche_tray()->get_initializer_webview_for_test());

  eche_tray()->OnConnectionStatusChanged(
      ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 1u);
  EXPECT_TRUE(eche_tray()->get_initializer_webview_for_test());
}

}  // namespace ash
