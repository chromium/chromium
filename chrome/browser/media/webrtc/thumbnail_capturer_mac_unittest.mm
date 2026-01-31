// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/thumbnail_capturer_mac.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <objc/runtime.h>

#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "media/capture/video/mac/test/screen_capture_kit_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/webrtc/modules/desktop_capture/mac/desktop_frame_utils.h"

namespace {

constexpr uint32_t kWindowId = 123;
constexpr uint32_t kDisplayId = 1;
constexpr uint32_t kPipWindowId = 999;

// The default max frame rate is 1fps, so the interval is 1s.
// We wait slightly longer to ensure the timer fires.
constexpr base::TimeDelta kRecurrentCaptureWaitTime = base::Seconds(1.1);

// Globals used to pass data between tests and swizzled methods.
// `g_simulated_windows` and `g_simulated_displays` are inputs to the swizzled
// API. `g_last_excluded_windows` is an output captured from the swizzled API.
// WARNING: This pattern is NOT thread-safe for parallel test execution within
// the same process. Ensure tests run sequentially.
NSArray* g_simulated_windows = nil;
NSArray* g_simulated_displays = nil;
NSArray* g_last_excluded_windows = nil;

}  // namespace

// --- Swizzling Categories ----------------------------------------------------

// 1. Swizzle SCShareableContent to return our fake windows/displays.
@interface SCShareableContent (ThumbnailCapturerMacTest)
+ (void)fakeGetShareableContentExcludingDesktopWindows:(BOOL)excludeDesktop
                                   onScreenWindowsOnly:(BOOL)onScreenOnly
                                     completionHandler:
                                         (void (^)(SCShareableContent*,
                                                   NSError*))completionHandler;
@end

@implementation SCShareableContent (ThumbnailCapturerMacTest)
+ (void)fakeGetShareableContentExcludingDesktopWindows:(BOOL)excludeDesktop
                                   onScreenWindowsOnly:(BOOL)onScreenOnly
                                     completionHandler:
                                         (void (^)(SCShareableContent*,
                                                   NSError*))completionHandler {
  NSArray* windows = g_simulated_windows ? g_simulated_windows : @[];
  NSArray* displays = g_simulated_displays ? g_simulated_displays : @[];

  id content = [[FakeSCShareableContent alloc]
      initWithWindows:(NSArray<FakeSCWindow*>*)windows
             displays:(NSArray<FakeSCDisplay*>*)displays];
  completionHandler(content, nil);
}
@end

// 2. Swizzle SCContentFilter to accept our Fake objects without crashing.
//    We swizzle the init methods to return 'self' (or a valid dummy) and ignore
//    the invalid input arguments.
@interface SCContentFilter (ThumbnailCapturerMacTest)
- (instancetype)initFakeWithDisplay:(id)display
                   excludingWindows:(NSArray*)windows;
- (instancetype)initFakeWithDesktopIndependentWindow:(id)window;
@end

@implementation SCContentFilter (ThumbnailCapturerMacTest)
- (instancetype)initFakeWithDisplay:(id)display
                   excludingWindows:(NSArray*)windows {
  g_last_excluded_windows = windows;
  return [super init];
}
- (instancetype)initFakeWithDesktopIndependentWindow:(id)window {
  g_last_excluded_windows = nil;
  return [super init];
}
@end

// 3. Swizzle SCScreenshotManager to return a dummy CGImage.
@interface SCScreenshotManager (ThumbnailCapturerMacTest)
+ (void)fakeCaptureImageWithFilter:(id)filter
                     configuration:(SCStreamConfiguration*)config
                 completionHandler:
                     (void (^)(CGImageRef, NSError*))completionHandler;
@end

@implementation SCScreenshotManager (ThumbnailCapturerMacTest)
+ (void)fakeCaptureImageWithFilter:(id)filter
                     configuration:(SCStreamConfiguration*)config
                 completionHandler:
                     (void (^)(CGImageRef, NSError*))completionHandler {
  // Create a 10x10 red image to simulate capture.
  int width = 10;
  int height = 10;
  int bytes_per_row = width * 4;
  std::vector<uint8_t> data(height * bytes_per_row, 0);

  // Fill with Red (RGBA: 255, 0, 0, 255)
  for (int i = 0; i < height * width; ++i) {
    data[i * 4 + 0] = 255;  // R
    data[i * 4 + 1] = 0;    // G
    data[i * 4 + 2] = 0;    // B
    data[i * 4 + 3] = 255;  // A
  }

  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  base::apple::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      data.data(), width, height, 8, bytes_per_row, color_space.get(),
      static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) |
          static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
  base::apple::ScopedCFTypeRef<CGImageRef> image(
      CGBitmapContextCreateImage(context.get()));

  if (completionHandler) {
    completionHandler(image.get(), nil);
  }
}
@end

// --- Test Fixture ------------------------------------------------------------

namespace {

using ::testing::_;
using ::testing::NiceMock;

class MockConsumer : public ThumbnailCapturer::Consumer {
 public:
  MOCK_METHOD(void,
              OnRecurrentCaptureResult,
              (ThumbnailCapturer::Result result,
               std::unique_ptr<webrtc::DesktopFrame> frame,
               ThumbnailCapturer::SourceId source_id),
              (override));
  MOCK_METHOD(void, OnSourceListUpdated, (), (override));
  MOCK_METHOD(void,
              OnCaptureResult,
              (ThumbnailCapturer::Result result,
               std::unique_ptr<webrtc::DesktopFrame> frame),
              (override));
};

class ThumbnailCapturerMacTest : public testing::Test {
 public:
  ThumbnailCapturerMacTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    // These tests rely on MacOS 14.4+ features (ScreenshotManagerCapturer).
    if (@available(macOS 14.4, *)) {
      // 1. Swizzle SCShareableContent
      shareable_content_swizzler_ =
          std::make_unique<base::apple::ScopedObjCClassSwizzler>(
              [SCShareableContent class],
              @selector
              (getShareableContentExcludingDesktopWindows:
                                      onScreenWindowsOnly:completionHandler:),
              @selector
              (fakeGetShareableContentExcludingDesktopWindows:
                                          onScreenWindowsOnly:completionHandler
                                                             :));

      // 2. Swizzle SCContentFilter inits
      // We must swizzle these because we pass FakeSCWindow/Display objects
      // to them, and the real implementation would crash.
      content_filter_window_swizzler_ =
          std::make_unique<base::apple::ScopedObjCClassSwizzler>(
              [SCContentFilter class],
              @selector(initWithDesktopIndependentWindow:),
              @selector(initFakeWithDesktopIndependentWindow:));

      content_filter_display_swizzler_ =
          std::make_unique<base::apple::ScopedObjCClassSwizzler>(
              [SCContentFilter class],
              @selector(initWithDisplay:excludingWindows:),
              @selector(initFakeWithDisplay:excludingWindows:));

      // 3. Swizzle SCScreenshotManager
      screenshot_manager_swizzler_ =
          std::make_unique<base::apple::ScopedObjCClassSwizzler>(
              [SCScreenshotManager class],
              @selector(captureImageWithFilter:
                                 configuration:completionHandler:),
              @selector(fakeCaptureImageWithFilter:
                                     configuration:completionHandler:));
    } else {
      GTEST_SKIP() << "Skipping tests on macOS < 14.4.";
    }
  }

  void TearDown() override {
    capturer_.reset();

    // Reset Swizzlers.
    shareable_content_swizzler_.reset();
    content_filter_window_swizzler_.reset();
    content_filter_display_swizzler_.reset();
    screenshot_manager_swizzler_.reset();

    // Clear globals.
    g_simulated_windows = nil;
    g_simulated_displays = nil;
    g_last_excluded_windows = nil;
  }

  void SetSimulatedWindows(NSArray* windows) { g_simulated_windows = windows; }
  void SetSimulatedDisplays(NSArray* displays) {
    g_simulated_displays = displays;
  }

  void InitializeWindowCapturer() {
    capturer_ = CreateThumbnailCapturerMacForTesting(
        DesktopMediaList::Type::kWindow, content::GlobalRenderFrameHostId());
  }

  void InitializeScreenCapturerWithPip(bool pip_owner_matches_capturer) {
    content::WebContents* capturer_wc =
        web_contents_factory_.CreateWebContents(&profile_);
    content::WebContents* pip_wc =
        pip_owner_matches_capturer
            ? capturer_wc
            : web_contents_factory_.CreateWebContents(&profile_);

    content::GlobalRenderFrameHostId rfh_id =
        capturer_wc->GetPrimaryMainFrame()->GetGlobalId();

    capturer_ = CreateThumbnailCapturerMacForTesting(
        DesktopMediaList::Type::kScreen, rfh_id,
        base::BindLambdaForTesting([pip_wc]() { return pip_wc; }),
        base::BindLambdaForTesting(
            [](content::DesktopMediaID::Id)
                -> std::optional<content::DesktopMediaID::Id> {
              return kPipWindowId;
            }));
  }

  void SetSimulatedWindowAndDisplay(int window_id, int display_id = 0) {
    FakeSCRunningApplication* app =
        [[FakeSCRunningApplication alloc] initWithProcessID:100
                                            applicationName:@"Fake App"
                                           bundleIdentifier:@"com.test.app"];
    FakeSCWindow* window =
        [[FakeSCWindow alloc] initWithID:window_id
                                   title:@"Fake Window"
                       owningApplication:app
                             windowLayer:0
                                   frame:CGRectMake(0, 0, 100, 100)
                                onScreen:YES];

    SetSimulatedWindows(@[ window ]);

    if (display_id > 0) {
      FakeSCDisplay* display =
          [[FakeSCDisplay alloc] initWithID:display_id
                                      frame:CGRectMake(0, 0, 1920, 1080)];
      SetSimulatedDisplays(@[ display ]);
    }
  }

  void StartCapturer() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(mock_consumer_, OnSourceListUpdated())
        .WillRepeatedly(base::test::RunClosure(run_loop.QuitClosure()));
    capturer_->Start(&mock_consumer_);
    run_loop.Run();
  }

  void AdvanceClockAndExpectSourceListUpdate(base::TimeDelta delta) {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(mock_consumer_, OnSourceListUpdated())
        .WillRepeatedly(base::test::RunClosure(run_loop.QuitClosure()));
    task_environment_.AdvanceClock(delta);
    run_loop.Run();
  }

  void SelectSource(ThumbnailCapturer::SourceId id) {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_consumer_,
                OnRecurrentCaptureResult(ThumbnailCapturer::Result::SUCCESS,
                                         testing::NotNull(), id))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

    capturer_->SelectSources({id}, gfx::Size(100, 100));
    run_loop.Run();
  }

  void AdvanceClockAndExpectCaptureResult(ThumbnailCapturer::SourceId id) {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(mock_consumer_,
                OnRecurrentCaptureResult(ThumbnailCapturer::Result::SUCCESS,
                                         testing::NotNull(), id))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

    task_environment_.AdvanceClock(kRecurrentCaptureWaitTime);
    run_loop.Run();
  }

 protected:
  // Use MOCK_TIME to control the recurring capture timer.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<ThumbnailCapturer> capturer_;
  NiceMock<MockConsumer> mock_consumer_;

  // Swizzlers
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      shareable_content_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      content_filter_window_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      content_filter_display_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      screenshot_manager_swizzler_;

  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
};

// Test that UpdateWindowsList calls OnSourceListUpdated when content is
// received and filters out invalid windows.
TEST_F(ThumbnailCapturerMacTest, UpdateWindowsList) {
  FakeSCRunningApplication* app =
      [[FakeSCRunningApplication alloc] initWithProcessID:100
                                          applicationName:@"Fake App"
                                         bundleIdentifier:@"com.test.app"];
  FakeSCWindow* valid_window =
      [[FakeSCWindow alloc] initWithID:kWindowId
                                 title:@"Valid Window"
                     owningApplication:app
                           windowLayer:0
                                 frame:CGRectMake(0, 0, 100, 100)
                              onScreen:YES];

  // Window with layer != 0 (e.g. dialog/overlay) should be excluded.
  FakeSCWindow* dialog_window =
      [[FakeSCWindow alloc] initWithID:456
                                 title:@"Dialog Window"
                     owningApplication:app
                           windowLayer:1
                                 frame:CGRectMake(50, 50, 100, 100)
                              onScreen:YES];

  // Window smaller than kThumbnailCapturerMacMinWindowSize (40) should be
  // excluded.
  FakeSCWindow* small_window =
      [[FakeSCWindow alloc] initWithID:789
                                 title:@"Small Window"
                     owningApplication:app
                           windowLayer:0
                                 frame:CGRectMake(0, 0, 10, 10)
                              onScreen:YES];

  SetSimulatedWindows(@[ valid_window, dialog_window, small_window ]);

  InitializeWindowCapturer();
  StartCapturer();

  ThumbnailCapturer::SourceList sources;
  capturer_->GetSourceList(&sources);
  EXPECT_THAT(
      sources,
      testing::ElementsAre(testing::AllOf(
          testing::Field(&ThumbnailCapturer::Source::id, kWindowId),
          testing::Field(&ThumbnailCapturer::Source::title, "Valid Window"))));
}

// Test that selecting a source triggers the ScreenshotManagerCapturer logic
// and results in a captured frame callback.
TEST_F(ThumbnailCapturerMacTest, SelectSourcesAndCapture) {
  SetSimulatedWindowAndDisplay(kWindowId);

  InitializeWindowCapturer();
  StartCapturer();

  SelectSource(kWindowId);

  AdvanceClockAndExpectCaptureResult(kWindowId);
}

// Test that the PiP window is excluded from the list of windows to capture
// when the capturer is the PiP owner.
TEST_F(ThumbnailCapturerMacTest, CaptureScreen_PipWindowExcluded) {
  SetSimulatedWindowAndDisplay(kPipWindowId, kDisplayId);

  InitializeScreenCapturerWithPip(/*pip_owner_matches_capturer=*/true);
  StartCapturer();

  // Wait for second update to ensure PiP IDs are fetched (requires displays).
  AdvanceClockAndExpectSourceListUpdate(base::Milliseconds(300));

  SelectSource(kDisplayId);

  // Verify g_last_excluded_windows contains PiP window.
  NSArray<FakeSCWindow*>* expected_excluded_windows = [g_simulated_windows
      filteredArrayUsingPredicate:[NSPredicate
                                      predicateWithFormat:@"windowID == %u",
                                                          kPipWindowId]];
  EXPECT_NSEQ(expected_excluded_windows, g_last_excluded_windows);
}

// Test that the PiP window is not excluded from the list of windows to capture
// when the capturer is not the PiP owner.
TEST_F(ThumbnailCapturerMacTest, CaptureScreen_PipWindowNotExcluded) {
  SetSimulatedWindowAndDisplay(kPipWindowId, kDisplayId);

  InitializeScreenCapturerWithPip(/*pip_owner_matches_capturer=*/false);
  StartCapturer();

  // Wait for second update to ensure PiP IDs are fetched (requires displays).
  AdvanceClockAndExpectSourceListUpdate(base::Milliseconds(300));

  SelectSource(kDisplayId);

  // Verify g_last_excluded_windows is empty because owner didn't match.
  EXPECT_NSEQ(@[], g_last_excluded_windows);
}

}  // namespace
