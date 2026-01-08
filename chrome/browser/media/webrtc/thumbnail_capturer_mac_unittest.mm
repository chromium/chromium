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
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/webrtc/modules/desktop_capture/mac/desktop_frame_utils.h"

namespace {

// The default max frame rate is 1fps, so the interval is 1s.
// We wait slightly longer to ensure the timer fires.
constexpr base::TimeDelta kRecurrentCaptureWaitTime = base::Seconds(1.1);

// Globals used to pass data into the swizzled methods.
// WARNING: This pattern is NOT thread-safe for parallel test execution within
// the same process. Ensure tests run sequentially.
NSArray* g_simulated_windows = nil;
NSArray* g_simulated_displays = nil;

}  // namespace

// --- Fake Classes ------------------------------------------------------------

// Fake classes to avoid mocking system classes which can be unstable or
// unavailable in test environments (e.g., restricted by TCC).

@interface FakeSCRunningApplication : NSObject
@property(readonly) pid_t processID;
@property(readonly) NSString* applicationName;
- (instancetype)initWithProcessID:(pid_t)pid applicationName:(NSString*)name;
@end

@implementation FakeSCRunningApplication
@synthesize processID = _processID;
@synthesize applicationName = _applicationName;
- (instancetype)initWithProcessID:(pid_t)pid applicationName:(NSString*)name {
  if (self = [super init]) {
    _processID = pid;
    _applicationName = name;
  }
  return self;
}
@end

@interface FakeSCWindow : NSObject
@property(readonly) CGWindowID windowID;
@property(readonly) CGRect frame;
@property(readonly) NSString* title;
@property(readonly) NSInteger windowLayer;
@property(readonly) FakeSCRunningApplication* owningApplication;
- (instancetype)initWithID:(CGWindowID)wid
                     title:(NSString*)title
         owningApplication:(FakeSCRunningApplication*)app
               windowLayer:(NSInteger)layer
                     frame:(CGRect)frame;
@end

@implementation FakeSCWindow
@synthesize windowID = _windowID;
@synthesize frame = _frame;
@synthesize title = _title;
@synthesize windowLayer = _windowLayer;
@synthesize owningApplication = _owningApplication;

- (instancetype)initWithID:(CGWindowID)wid
                     title:(NSString*)title
         owningApplication:(FakeSCRunningApplication*)app
               windowLayer:(NSInteger)layer
                     frame:(CGRect)frame {
  if (self = [super init]) {
    _windowID = wid;
    _title = title;
    _owningApplication = app;
    _windowLayer = layer;
    _frame = frame;
  }
  return self;
}
@end

@interface FakeSCDisplay : NSObject
@property(readonly) CGDirectDisplayID displayID;
@property(readonly) CGRect frame;
- (instancetype)initWithID:(CGDirectDisplayID)did frame:(CGRect)frame;
@end

@implementation FakeSCDisplay
@synthesize displayID = _displayID;
@synthesize frame = _frame;
- (instancetype)initWithID:(CGDirectDisplayID)did frame:(CGRect)frame {
  if (self = [super init]) {
    _displayID = did;
    _frame = frame;
  }
  return self;
}
@end

@interface FakeSCShareableContent : NSObject
@property(readonly) NSArray<FakeSCWindow*>* windows;
@property(readonly) NSArray<FakeSCDisplay*>* displays;
- (instancetype)initWithWindows:(NSArray<FakeSCWindow*>*)windows
                       displays:(NSArray<FakeSCDisplay*>*)displays;
@end

@implementation FakeSCShareableContent
@synthesize windows = _windows;
@synthesize displays = _displays;
- (instancetype)initWithWindows:(NSArray<FakeSCWindow*>*)windows
                       displays:(NSArray<FakeSCDisplay*>*)displays {
  if (self = [super init]) {
    _windows = windows;
    _displays = displays;
  }
  return self;
}
@end

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
- (instancetype)fakeInitWithDisplay:(id)display
                   excludingWindows:(NSArray*)windows;
- (instancetype)fakeInitWithDesktopIndependentWindow:(id)window;
@end

@implementation SCContentFilter (ThumbnailCapturerMacTest)
- (instancetype)fakeInitWithDisplay:(id)display
                   excludingWindows:(NSArray*)windows {
  return [super init];
}
- (instancetype)fakeInitWithDesktopIndependentWindow:(id)window {
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
              @selector(fakeInitWithDesktopIndependentWindow:));

      content_filter_display_swizzler_ =
          std::make_unique<base::apple::ScopedObjCClassSwizzler>(
              [SCContentFilter class],
              @selector(initWithDisplay:excludingWindows:),
              @selector(fakeInitWithDisplay:excludingWindows:));

      // 3. Swizzle SCScreenshotManager
      screenshot_manager_swizzler_ =
          std::make_unique<base::apple::ScopedObjCClassSwizzler>(
              [SCScreenshotManager class],
              @selector(captureImageWithFilter:
                                 configuration:completionHandler:),
              @selector(fakeCaptureImageWithFilter:
                                     configuration:completionHandler:));

      capturer_ =
          CreateThumbnailCapturerMacForTesting(DesktopMediaList::Type::kWindow);
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
  }

  void SetSimulatedWindows(NSArray* windows) { g_simulated_windows = windows; }

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
};

// Test that UpdateWindowsList calls OnSourceListUpdated when content is
// received.
TEST_F(ThumbnailCapturerMacTest, UpdateWindowsList) {
  FakeSCRunningApplication* app =
      [[FakeSCRunningApplication alloc] initWithProcessID:100
                                          applicationName:@"Fake App"];
  FakeSCWindow* valid_window =
      [[FakeSCWindow alloc] initWithID:123
                                 title:@"Valid Window"
                     owningApplication:app
                           windowLayer:0
                                 frame:CGRectMake(0, 0, 100, 100)];

  // Window with layer != 0 (e.g. dialog/overlay) should be excluded.
  FakeSCWindow* dialog_window =
      [[FakeSCWindow alloc] initWithID:456
                                 title:@"Dialog Window"
                     owningApplication:app
                           windowLayer:1
                                 frame:CGRectMake(50, 50, 100, 100)];

  // Window smaller than kThumbnailCapturerMacMinWindowSize (40) should be
  // excluded.
  FakeSCWindow* small_window =
      [[FakeSCWindow alloc] initWithID:789
                                 title:@"Small Window"
                     owningApplication:app
                           windowLayer:0
                                 frame:CGRectMake(0, 0, 10, 10)];

  SetSimulatedWindows(@[ valid_window, dialog_window, small_window ]);

  // We expect OnSourceListUpdated to be called.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  EXPECT_CALL(mock_consumer_, OnSourceListUpdated())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  capturer_->Start(&mock_consumer_);

  // Process the tasks (including the initial timer fire for
  // UpdateWindowsList)
  run_loop.Run();

  ThumbnailCapturer::SourceList sources;
  capturer_->GetSourceList(&sources);
  EXPECT_THAT(
      sources,
      testing::ElementsAre(testing::AllOf(
          testing::Field(&ThumbnailCapturer::Source::id, 123),
          testing::Field(&ThumbnailCapturer::Source::title, "Valid Window"))));
}

// Test that selecting a source triggers the ScreenshotManagerCapturer logic
// and results in a captured frame callback.
TEST_F(ThumbnailCapturerMacTest, SelectSourcesAndCapture) {
  const int kWindowId = 123;

  // 1. Setup Environment.
  FakeSCRunningApplication* app =
      [[FakeSCRunningApplication alloc] initWithProcessID:100
                                          applicationName:@"Fake App"];
  FakeSCWindow* window =
      [[FakeSCWindow alloc] initWithID:kWindowId
                                 title:@"Fake Window"
                     owningApplication:app
                           windowLayer:0
                                 frame:CGRectMake(0, 0, 100, 100)];
  SetSimulatedWindows(@[ window ]);

  // 2. Wait for initial source list update.
  {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(mock_consumer_, OnSourceListUpdated())
        .WillRepeatedly(base::test::RunClosure(run_loop.QuitClosure()));
    capturer_->Start(&mock_consumer_);
    run_loop.Run();
  }

  // 3. Select the source.
  std::vector<ThumbnailCapturer::SourceId> ids = {kWindowId};

  // 4. Run loop to process the capture.
  // The ScreenshotManagerCapturer calls BindPostTask, so we need to run the
  // loop to receive the callback on the correct thread.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_consumer_,
                OnRecurrentCaptureResult(ThumbnailCapturer::Result::SUCCESS,
                                         testing::NotNull(), kWindowId))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

    capturer_->SelectSources(ids, gfx::Size(100, 100));
    run_loop.Run();
  }

  // 5. Verify Recurrent Capture.
  // Advance time to trigger the next timer tick.
  {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(mock_consumer_,
                OnRecurrentCaptureResult(ThumbnailCapturer::Result::SUCCESS,
                                         testing::NotNull(), kWindowId))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

    task_environment_.AdvanceClock(kRecurrentCaptureWaitTime);
    run_loop.Run();
  }
}

}  // namespace
