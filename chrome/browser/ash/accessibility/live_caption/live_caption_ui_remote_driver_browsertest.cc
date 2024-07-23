// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test lives in ::ash (v.s. the live_caption component) because it
// exercises as much of the UI stack as possible, which includes e.g. enabling
// Ash-specific features.

#include "components/live_caption/live_caption_ui_remote_driver.h"

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/live_caption/views/caption_bubble_controller_views.h"
#include "components/soda/soda_installer.h"
#include "content/public/test/browser_test.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using media::mojom::SpeechRecognitionRecognizerClient;
using media::mojom::SpeechRecognitionSurface;
using media::mojom::SpeechRecognitionSurfaceClient;
using testing::_;

class MockSurface : public SpeechRecognitionSurface {
 public:
  MockSurface() = default;
  ~MockSurface() override = default;

  MockSurface(const MockSurface&) = delete;
  MockSurface& operator=(const MockSurface&) = delete;

  // Establish ourselves as the implementation of the surface, and grab a handle
  // to the remote surface client. For good measure, hold the given recognizer
  // client remote, since its self-owned implementation will destroy itself
  // unless we do.
  void Bind(mojo::PendingReceiver<SpeechRecognitionSurface> receiver,
            mojo::PendingRemote<SpeechRecognitionSurfaceClient> surface_client,
            mojo::PendingRemote<SpeechRecognitionRecognizerClient>
                recognizer_client) {
    receiver_.Bind(std::move(receiver));
    surface_client_.Bind(std::move(surface_client));
    recognizer_client_.Bind(std::move(recognizer_client));
  }

  mojo::Remote<SpeechRecognitionRecognizerClient>& remote_recognizer_client() {
    return recognizer_client_;
  }

  // media::mojom::SpeechRecognitionSurface:
  MOCK_METHOD(void, Activate, (), (override));
  MOCK_METHOD(void,
              GetBounds,
              (SpeechRecognitionSurface::GetBoundsCallback),
              (override));

 private:
  mojo::Receiver<SpeechRecognitionSurface> receiver_{this};
  mojo::Remote<SpeechRecognitionSurfaceClient> surface_client_;

  // Must hold this to keep the UI driver alive.
  mojo::Remote<SpeechRecognitionRecognizerClient> recognizer_client_;
};

class LiveCaptionUiRemoteDriverTest : public InProcessBrowserTest {
 public:
  LiveCaptionUiRemoteDriverTest()
      : scoped_feature_list_(features::kOnDeviceSpeechRecognition) {}
  ~LiveCaptionUiRemoteDriverTest() override = default;

  LiveCaptionUiRemoteDriverTest(const LiveCaptionUiRemoteDriverTest&) = delete;
  LiveCaptionUiRemoteDriverTest& operator=(
      const LiveCaptionUiRemoteDriverTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                                 true);

    // Don't actually try to download SODA.
    speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();

    // Create bubble UI and grab a handle to it.
    controller_ = captions::LiveCaptionControllerFactory::GetForProfile(
        browser()->profile());
    base::RunLoop().RunUntilIdle();
  }

  // Test-only getters.

  captions::CaptionBubbleControllerViews* bubble_controller() const {
    return static_cast<captions::CaptionBubbleControllerViews*>(
        controller_->caption_bubble_controller_for_testing());
  }

  captions::CaptionBubble* bubble() {
    return bubble_controller()->GetCaptionBubbleForTesting();
  }

  bool IsWidgetVisible() const {
    return bubble_controller() &&
           bubble_controller()->IsWidgetVisibleForTesting();
  }

  std::string GetBubbleText() const {
    return bubble_controller()
               ? bubble_controller()->GetBubbleLabelTextForTesting()
               : "";
  }

  // Create a new UI driver that communicates with the provided mock surface.
  // The driver will destroy itself its connection to the surface drops.
  captions::LiveCaptionUiRemoteDriver* NewUiDriverForSurface(
      MockSurface* surface,
      bool stub_bounds) {
    // Bind the fake lacros ends of the Mojo pipes.
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        client_receiver;
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSurfaceClient>
        surface_client_receiver;
    mojo::PendingRemote<media::mojom::SpeechRecognitionSurface> surface_remote;
    surface->Bind(surface_remote.InitWithNewPipeAndPassReceiver(),
                  surface_client_receiver.InitWithNewPipeAndPassRemote(),
                  client_receiver.InitWithNewPipeAndPassRemote());

    // Sending an initial transcription will trigger a bounds query that must be
    // replied to. The expectation is optionally added here since it is used in
    // many tests.
    if (stub_bounds) {
      EXPECT_CALL(*surface, GetBounds(_))
          .WillOnce(
              [&](auto cb) { std::move(cb).Run(gfx::Rect(1, 1, 600, 800)); })
          .RetiresOnSaturation();
    }

    // Create a driver and bind the Ash ends of the Mojo pipes.
    auto driver = std::make_unique<captions::LiveCaptionUiRemoteDriver>(
        controller_, std::move(surface_client_receiver),
        std::move(surface_remote), "session-id");
    captions::LiveCaptionUiRemoteDriver* raw_driver = driver.get();

    mojo::MakeSelfOwnedReceiver(std::move(driver), std::move(client_receiver));
    return raw_driver;
  }

  // Emulate new transcriptions being generated for the given surface and wait
  // until the recognizer client has responded.
  bool EmitTranscribedText(MockSurface* surface, const std::string& text) {
    bool result = false;
    surface->remote_recognizer_client()->OnSpeechRecognitionRecognitionEvent(
        media::SpeechRecognitionResult(text, /*is_final=*/false),
        base::BindLambdaForTesting([&](bool r) { result = r; }));
    base::RunLoop().RunUntilIdle();
    return result;
  }

  // Emulate clicking the given button with the mouse.
  void ClickButton(views::Button* button) {
    button->OnMousePressed(ui::MouseEvent(
        ui::EventType::kMousePressed, gfx::Point(0, 0), gfx::Point(0, 0),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    button->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMouseReleased, gfx::Point(0, 0), gfx::Point(0, 0),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

 protected:
  raw_ptr<captions::LiveCaptionController, DanglingUntriaged> controller_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that captions sent from lacros are shown in the bubble.
IN_PROC_BROWSER_TEST_F(LiveCaptionUiRemoteDriverTest, DisplaysText) {
  MockSurface surface;
  NewUiDriverForSurface(&surface, /*stub_bounds=*/true);

  // Emitting text should cause the bubble to appear.
  EXPECT_TRUE(EmitTranscribedText(&surface, "Test text"));
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Test text", GetBubbleText());

  // Emitting empty text should cause the bubble to disappear.
  EXPECT_TRUE(EmitTranscribedText(&surface, ""));
  EXPECT_FALSE(IsWidgetVisible());

  // Bubble should be shown again when non-empty text is emitted.
  EXPECT_TRUE(EmitTranscribedText(&surface, "Test text 2"));
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Test text 2", GetBubbleText());
}

// Test that two media sources are both handled by the bubble.
IN_PROC_BROWSER_TEST_F(LiveCaptionUiRemoteDriverTest, MultipleSources) {
  MockSurface surface_1;
  NewUiDriverForSurface(&surface_1, /*stub_bounds=*/true);

  MockSurface surface_2;
  NewUiDriverForSurface(&surface_2, /*stub_bounds=*/false);

  // Text from surface 1 should be shown.
  EXPECT_TRUE(EmitTranscribedText(&surface_1, "Surface 1 text"));
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Surface 1 text", GetBubbleText());

  // Text from surface 2 should replace surface 1's text because it is newer.
  EXPECT_TRUE(EmitTranscribedText(&surface_2, "Surface 2 text"));
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Surface 2 text", GetBubbleText());

  // Back to surface 1.
  EXPECT_TRUE(EmitTranscribedText(&surface_1, "More surface 1 text"));
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("More surface 1 text", GetBubbleText());
}

// Test that bubble is placed in correct position.
IN_PROC_BROWSER_TEST_F(LiveCaptionUiRemoteDriverTest, BubblePosition) {
  MockSurface surface;
  NewUiDriverForSurface(&surface, /*stub_bounds=*/false);

  // Set browser window to a known size and have the surface report the size.
  const gfx::Rect window_bounds(10, 10, 800, 600);
  browser()->window()->SetBounds(window_bounds);
  EXPECT_CALL(surface, GetBounds(_))
      .WillOnce([&](auto cb) { std::move(cb).Run(window_bounds); })
      .RetiresOnSaturation();
  base::RunLoop().RunUntilIdle();
  const gfx::Rect context_rect = views::Widget::GetWidgetForNativeWindow(
                                     browser()->window()->GetNativeWindow())
                                     ->GetClientAreaBoundsInScreen();

  // Trigger positioning via first emission of text.
  EXPECT_TRUE(EmitTranscribedText(&surface, "Test text"));
  EXPECT_TRUE(IsWidgetVisible());

  // Reuses the positioning logic from
  // `CaptionBubbleControllerViewsTest::BubblePositioning`.
  const int bubble_width = 536;
  const int bubble_y_offset = 20;
  const gfx::Insets bubble_margins(6);
  const gfx::Rect bubble_bounds = bubble_controller()
                                      ->GetCaptionWidgetForTesting()
                                      ->GetWindowBoundsInScreen();

  // There may be some rounding errors as we do floating point math with ints.
  // Check that points are almost the same.
  EXPECT_LT(
      abs(bubble_bounds.CenterPoint().x() - context_rect.CenterPoint().x()), 2);
  EXPECT_EQ(bubble_bounds.bottom(), context_rect.bottom() - bubble_y_offset);
  EXPECT_EQ(bubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(bubble()->margins(), bubble_margins);
}

// Test that an error is shown when reported by the surface.
IN_PROC_BROWSER_TEST_F(LiveCaptionUiRemoteDriverTest, DisplaysError) {
  MockSurface surface;
  NewUiDriverForSurface(&surface, /*stub_bounds=*/true);

  // Bubble should be shown initially.
  EXPECT_TRUE(EmitTranscribedText(&surface, "Test text"));
  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_NE(nullptr, bubble_controller());
  EXPECT_FALSE(bubble_controller()->IsGenericErrorMessageVisibleForTesting());

  // Error should trigger error message.
  surface.remote_recognizer_client()->OnSpeechRecognitionError();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_NE(nullptr, bubble_controller());
  EXPECT_TRUE(bubble_controller()->IsGenericErrorMessageVisibleForTesting());

  // Receiving new text during an error should cause the error to disappear.
  EXPECT_TRUE(EmitTranscribedText(&surface, "More test text"));
  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_NE(nullptr, bubble_controller());
  EXPECT_FALSE(bubble_controller()->IsGenericErrorMessageVisibleForTesting());
}

// Test that the bubble is hidden when a stream end is reported.
IN_PROC_BROWSER_TEST_F(LiveCaptionUiRemoteDriverTest, StreamEnd) {
  MockSurface surface;
  NewUiDriverForSurface(&surface, /*stub_bounds=*/true);

  // Emitting text should cause the bubble to appear.
  EXPECT_TRUE(EmitTranscribedText(&surface, "Test text"));
  EXPECT_TRUE(IsWidgetVisible());

  // Send stream end signal.
  surface.remote_recognizer_client()->OnSpeechRecognitionStopped();
  base::RunLoop().RunUntilIdle();

  // Bubble should have disappeared.
  ASSERT_NE(nullptr, bubble_controller());
  EXPECT_FALSE(IsWidgetVisible());
}

// Test that closing the bubble ends transcription until a navigation.
IN_PROC_BROWSER_TEST_F(LiveCaptionUiRemoteDriverTest, CloseBubble) {
  MockSurface surface_1, surface_2, surface_3;
  auto* driver_1 = NewUiDriverForSurface(&surface_1, /*stub_bounds=*/true);
  NewUiDriverForSurface(&surface_2, /*stub_bounds=*/false);
  NewUiDriverForSurface(&surface_3, /*stub_bounds=*/false);

  // Emitting text should cause the bubble to appear.
  EXPECT_TRUE(EmitTranscribedText(&surface_1, "Test text"));
  EXPECT_TRUE(IsWidgetVisible());

  // Close the bubble.
  ASSERT_NE(nullptr, bubble());
  ClickButton(bubble()->GetCloseButtonForTesting());
  EXPECT_FALSE(IsWidgetVisible());

  // Emitting further text should fail.
  EXPECT_FALSE(EmitTranscribedText(&surface_1, "More test text"));
  EXPECT_FALSE(IsWidgetVisible());

  // Emitting text from a different stream in the same closed session should
  // also fail.
  EXPECT_FALSE(EmitTranscribedText(&surface_2, "Surface 2 text"));
  EXPECT_FALSE(IsWidgetVisible());

  // Emulate a navigation (i.e. session end).
  driver_1->OnSessionEnded();

  // Text from a new page should not cause the bubble to reappear because
  // closing the bubble disables the feature.
  EXPECT_FALSE(EmitTranscribedText(&surface_3, "New page text"));
  EXPECT_FALSE(IsWidgetVisible());
}

// Test that the back to tab message is delivered.
IN_PROC_BROWSER_TEST_F(LiveCaptionUiRemoteDriverTest, BackToTab) {
  MockSurface surface_1;
  MockSurface surface_2;
  NewUiDriverForSurface(&surface_1, /*stub_bounds=*/true);
  NewUiDriverForSurface(&surface_2, /*stub_bounds=*/false);

  // We expect these activate calls when we toggle back and forth.
  EXPECT_CALL(surface_1, Activate()).Times(2).RetiresOnSaturation();
  EXPECT_CALL(surface_2, Activate()).RetiresOnSaturation();

  // Emit text from surface 1 to activate its model. Then use the "back to tab"
  // UI.
  EXPECT_TRUE(EmitTranscribedText(&surface_1, "Surface 1 text"));
  ClickButton(bubble()->GetBackToTabButtonForTesting());

  // Emit text from surface 2 to activate its model.
  EXPECT_TRUE(EmitTranscribedText(&surface_2, "Surface 2 text"));
  ClickButton(bubble()->GetBackToTabButtonForTesting());

  // Emit text from surface 1 again to reactivate its model.
  EXPECT_TRUE(EmitTranscribedText(&surface_1, "More surface 1 text"));
  ClickButton(bubble()->GetBackToTabButtonForTesting());

  // Surface 1 should be activated twice, and surface 2 once.
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace ash
