// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_video_picture_in_picture_window_controller_impl.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "chrome/browser/picture_in_picture/scoped_disallow_picture_in_picture.h"
#include "media/base/media_switches.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

typedef base::ScopedObservation<PictureInPictureWindowManager,
                                PictureInPictureWindowManager::Observer>
    PictureInPictureWindowManagerdObservation;

class MockPictureInPictureWindowManagerObserver
    : public PictureInPictureWindowManager::Observer {
 public:
  MOCK_METHOD(void, OnEnterPictureInPicture, (), (override));
};

class MockPictureInPictureWindowController
    : public content::PictureInPictureWindowController {
 public:
  MockPictureInPictureWindowController() = default;
  MockPictureInPictureWindowController(
      const MockPictureInPictureWindowController&) = delete;
  MockPictureInPictureWindowController& operator=(
      const MockPictureInPictureWindowController&) = delete;
  ~MockPictureInPictureWindowController() override = default;

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, FocusInitiator, (), (override));
  MOCK_METHOD(void, Close, (bool), (override));
  MOCK_METHOD(void, CloseAndFocusInitiator, (), (override));
  MOCK_METHOD(void, OnWindowDestroyed, (bool), (override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (override));
  MOCK_METHOD(std::optional<gfx::Rect>, GetWindowBounds, (), (override));
  MOCK_METHOD(content::WebContents*, GetChildWebContents, (), (override));
  MOCK_METHOD(std::optional<url::Origin>, GetOrigin, (), (override));
};

class PictureInPictureWindowManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SetContents(CreateTestWebContents());
    child_web_contents_ = CreateTestWebContents();

    auto mock_video_picture_in_picture_controller = std::make_unique<
        content::MockVideoPictureInPictureWindowControllerImpl>(web_contents());
    mock_video_picture_in_picture_controller_ =
        mock_video_picture_in_picture_controller.get();
    web_contents()->SetUserData(
        mock_video_picture_in_picture_controller->UserDataKey(),
        std::move(mock_video_picture_in_picture_controller));
  }

  void TearDown() override {
    mock_video_picture_in_picture_controller_ = nullptr;
    DeleteContents();
    child_web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::WebContents* child_web_contents() const {
    return child_web_contents_.get();
  }

  content::MockVideoPictureInPictureWindowControllerImpl*
  mock_video_picture_in_picture_controller() const {
    return mock_video_picture_in_picture_controller_.get();
  }

 private:
  std::unique_ptr<content::WebContents> child_web_contents_;
  raw_ptr<content::MockVideoPictureInPictureWindowControllerImpl>
      mock_video_picture_in_picture_controller_;
};

}  // namespace

TEST_F(PictureInPictureWindowManagerTest, RespectsMinAndMaxSize) {
  // The max window size should be 80% of the screen.
  display::Display display(/*id=*/1, gfx::Rect(0, 0, 1000, 1000));
  EXPECT_EQ(gfx::Size(800, 800),
            PictureInPictureWindowManager::GetMaximumWindowSize(display));

  // The initial bounds of the PiP window should respect that.
  blink::mojom::PictureInPictureWindowOptions pip_options;
  pip_options.width = 900;
  pip_options.height = 900;
  EXPECT_EQ(
      gfx::Size(800, 800),
      PictureInPictureWindowManager::GetInstance()
          ->CalculateInitialPictureInPictureWindowBounds(pip_options, display)
          .size());

  // The minimum size should also be respected.
  pip_options.width = 100;
  pip_options.height = 500;
  EXPECT_EQ(
      gfx::Size(240, 500),
      PictureInPictureWindowManager::GetInstance()
          ->CalculateInitialPictureInPictureWindowBounds(pip_options, display)
          .size());
}

TEST_F(PictureInPictureWindowManagerTest,
       ExitPictureInPictureReturnsFalseWhenThereIsNoWindow) {
  EXPECT_FALSE(
      PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture());
}

TEST_F(PictureInPictureWindowManagerTest,
       ExitPictureInPictureReturnsTrueAndClosesWindow) {
  MockPictureInPictureWindowController controller;
  PictureInPictureWindowManager::GetInstance()
      ->EnterPictureInPictureWithController(&controller);
  EXPECT_CALL(controller, Close(/*should_pause_video=*/false));
  EXPECT_TRUE(
      PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture());
}

TEST_F(PictureInPictureWindowManagerTest, OnEnterVideoPictureInPicture) {
  PictureInPictureWindowManager* picture_in_picture_window_manager =
      PictureInPictureWindowManager::GetInstance();

  EXPECT_CALL(*mock_video_picture_in_picture_controller(),
              SetOnWindowCreatedNotifyObserversCallback);
  picture_in_picture_window_manager->EnterVideoPictureInPicture(web_contents());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PictureInPictureWindowManagerTest, OnEnterDocumentPictureInPicture) {
  PictureInPictureWindowManager* picture_in_picture_window_manager =
      PictureInPictureWindowManager::GetInstance();
  MockPictureInPictureWindowManagerObserver observer;
  PictureInPictureWindowManagerdObservation observation{&observer};
  observation.Observe(picture_in_picture_window_manager);
  EXPECT_CALL(observer, OnEnterPictureInPicture).Times(1);

  picture_in_picture_window_manager->EnterDocumentPictureInPicture(
      web_contents(), child_web_contents());
}

TEST_F(PictureInPictureWindowManagerTest, DontShowAutoPipSettingUiWithoutPip) {
  PictureInPictureWindowManager* picture_in_picture_window_manager =
      PictureInPictureWindowManager::GetInstance();
  // There's no pip open, so expect no setting UI.
  EXPECT_FALSE(picture_in_picture_window_manager->GetOverlayView(
      /* anchor_view = */ nullptr, views::BubbleBorder::TOP_CENTER));
}

TEST_F(PictureInPictureWindowManagerTest,
       DontShowAutoPipSettingUiForNonAutoPip) {
  PictureInPictureWindowManager* picture_in_picture_window_manager =
      PictureInPictureWindowManager::GetInstance();
  picture_in_picture_window_manager->EnterDocumentPictureInPicture(
      web_contents(), child_web_contents());
  // This isn't auto-pip, so expect no overlay view.
  EXPECT_FALSE(picture_in_picture_window_manager->GetOverlayView(
      /* anchor_view = */ nullptr, views::BubbleBorder::TOP_CENTER));
}

TEST_F(PictureInPictureWindowManagerTest, CorrectTypesAreSupported) {
  EXPECT_TRUE(
      PictureInPictureWindowManager::IsSupportedForDocumentPictureInPicture(
          GURL("https://foo.com")));
  EXPECT_FALSE(
      PictureInPictureWindowManager::IsSupportedForDocumentPictureInPicture(
          GURL("http://foo.com")));
  EXPECT_TRUE(
      PictureInPictureWindowManager::IsSupportedForDocumentPictureInPicture(
          GURL("http://localhost")));
  EXPECT_TRUE(
      PictureInPictureWindowManager::IsSupportedForDocumentPictureInPicture(
          GURL("https://localhost")));
  EXPECT_TRUE(
      PictureInPictureWindowManager::IsSupportedForDocumentPictureInPicture(
          GURL("file://foo/com")));
  EXPECT_FALSE(
      PictureInPictureWindowManager::IsSupportedForDocumentPictureInPicture(
          GURL("blob://foo.com")));
  EXPECT_FALSE(
      PictureInPictureWindowManager::IsSupportedForDocumentPictureInPicture(
          GURL("")));
  EXPECT_FALSE(
      PictureInPictureWindowManager::IsSupportedForDocumentPictureInPicture(
          GURL("about:blank")));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(
      PictureInPictureWindowManager::IsSupportedForDocumentPictureInPicture(
          GURL("chrome-extension://foocom")));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(
      PictureInPictureWindowManager::IsSupportedForDocumentPictureInPicture(
          GURL("chrome://newtab")));
}

TEST_F(PictureInPictureWindowManagerTest, RecordsInitialSizeHistograms) {
  display::Display display(/*id=*/1, gfx::Rect(0, 0, 1000, 1000));

  {
    base::HistogramTester histogram_tester;

    // Simulate requesting a window that is 400x500px and takes up 20% of the
    // total screen area.
    blink::mojom::PictureInPictureWindowOptions pip_options;
    pip_options.width = 400;
    pip_options.height = 500;
    PictureInPictureWindowManager::GetInstance()
        ->CalculateInitialPictureInPictureWindowBounds(pip_options, display);

    // Requested size histograms should be properly recorded.
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedInitialWidth", 400, 1);
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedInitialHeight", 500, 1);
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedSizeToScreenRatio", 20, 1);
  }

  {
    base::HistogramTester histogram_tester;

    // Simulate requesting a window with zero size.
    blink::mojom::PictureInPictureWindowOptions pip_options;
    pip_options.width = 0;
    pip_options.height = 0;
    PictureInPictureWindowManager::GetInstance()
        ->CalculateInitialPictureInPictureWindowBounds(pip_options, display);

    // Requested size histograms should be properly recorded. A size of zero
    // should be recorded as 1 percent.
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedInitialWidth", 0, 1);
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedInitialHeight", 0, 1);
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedSizeToScreenRatio", 1, 1);
  }

  {
    base::HistogramTester histogram_tester;

    // Simulate requesting a window with an area larger than the whole screen.
    blink::mojom::PictureInPictureWindowOptions pip_options;
    pip_options.width = 2000;
    pip_options.height = 2000;
    PictureInPictureWindowManager::GetInstance()
        ->CalculateInitialPictureInPictureWindowBounds(pip_options, display);

    // Requested size histograms should be properly recorded. A size larger than
    // the whole screen should be recorded as 100 percent.
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedInitialWidth", 2000, 1);
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedInitialHeight", 2000, 1);
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedSizeToScreenRatio", 100, 1);
  }

  {
    base::HistogramTester histogram_tester;

    display::Display empty_display(/*id=*/2, gfx::Rect(0, 0, 0, 0));

    // Simulate requesting a window inside an empty display.
    blink::mojom::PictureInPictureWindowOptions pip_options;
    pip_options.width = 1000;
    pip_options.height = 1000;
    PictureInPictureWindowManager::GetInstance()
        ->CalculateInitialPictureInPictureWindowBounds(pip_options,
                                                       empty_display);

    // Requested size histograms should be properly recorded. If the display
    // size is empty, then we should get a ratio of 100 percent.
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedInitialWidth", 1000, 1);
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedInitialHeight", 1000, 1);
    histogram_tester.ExpectUniqueSample(
        "Media.DocumentPictureInPicture.RequestedSizeToScreenRatio", 100, 1);
  }
}

TEST_F(PictureInPictureWindowManagerTest, CanDisallowPictureInPicture) {
  {
    // Disallowing before opening a picture-in-picture window should close it.
    ScopedDisallowPictureInPicture disallow;

    PictureInPictureWindowManager::GetInstance()->EnterDocumentPictureInPicture(
        web_contents(), child_web_contents());

    // The close does not happen synchronously, so we run posted tasks.
    EXPECT_TRUE(web_contents()->HasPictureInPictureDocument());
    task_environment()->RunUntilIdle();
    EXPECT_FALSE(web_contents()->HasPictureInPictureDocument());
  }

  {
    // Disallowing after opening a picture-in-picture window should close it.
    PictureInPictureWindowManager::GetInstance()->EnterDocumentPictureInPicture(
        web_contents(), child_web_contents());

    EXPECT_TRUE(web_contents()->HasPictureInPictureDocument());
    ScopedDisallowPictureInPicture disallow;
    EXPECT_FALSE(web_contents()->HasPictureInPictureDocument());
  }

  {
    {
      ScopedDisallowPictureInPicture disallow1;

      {
        // Multiple ScopedDisallowPictureInPicture should still block
        // picture-in-picture windows.
        ScopedDisallowPictureInPicture disallow2;

        PictureInPictureWindowManager::GetInstance()
            ->EnterDocumentPictureInPicture(web_contents(),
                                            child_web_contents());

        EXPECT_TRUE(web_contents()->HasPictureInPictureDocument());
        task_environment()->RunUntilIdle();
        EXPECT_FALSE(web_contents()->HasPictureInPictureDocument());
      }

      // When one of them is destroyed but the other remains, it should still
      // block picture-in-picture windows.
      PictureInPictureWindowManager::GetInstance()
          ->EnterDocumentPictureInPicture(web_contents(), child_web_contents());

      EXPECT_TRUE(web_contents()->HasPictureInPictureDocument());
      task_environment()->RunUntilIdle();
      EXPECT_FALSE(web_contents()->HasPictureInPictureDocument());
    }

    // Once both have been destroyed, picture-in-picture windows should be
    // unblocked.
    PictureInPictureWindowManager::GetInstance()->EnterDocumentPictureInPicture(
        web_contents(), child_web_contents());

    EXPECT_TRUE(web_contents()->HasPictureInPictureDocument());
    task_environment()->RunUntilIdle();
    EXPECT_TRUE(web_contents()->HasPictureInPictureDocument());
  }
}

TEST_F(PictureInPictureWindowManagerTest,
       ShouldFileDialogBlockPictureInPicture) {
  PictureInPictureWindowManager::GetInstance()->EnterDocumentPictureInPicture(
      web_contents(), child_web_contents());

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(media::kFileDialogsBlockPictureInPicture);

    // With the feature enabled, file dialogs that aren't on a document
    // picture-in-picture window should block picture-in-picture windows.
    EXPECT_TRUE(PictureInPictureWindowManager::GetInstance()
                    ->ShouldFileDialogBlockPictureInPicture(web_contents()));
    EXPECT_FALSE(
        PictureInPictureWindowManager::GetInstance()
            ->ShouldFileDialogBlockPictureInPicture(child_web_contents()));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        media::kFileDialogsBlockPictureInPicture);

    // With the feature disabled, no file dialogs should block
    // picture-in-picture windows.
    EXPECT_FALSE(PictureInPictureWindowManager::GetInstance()
                     ->ShouldFileDialogBlockPictureInPicture(web_contents()));
    EXPECT_FALSE(
        PictureInPictureWindowManager::GetInstance()
            ->ShouldFileDialogBlockPictureInPicture(child_web_contents()));
  }
}

#endif  // !BUILDFLAG(IS_ANDROID)
