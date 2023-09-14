// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/thumbnail_capturer_mac.h"

#include <CoreGraphics/CoreGraphics.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <VideoToolbox/VideoToolbox.h>

#include <unordered_set>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/timer/timer.h"
#include "chrome/browser/media/webrtc/desktop_media_list_base.h"
#include "media/capture/video_capture_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/modules/desktop_capture/mac/desktop_frame_utils.h"

using SampleCallback =
    base::RepeatingCallback<void(CGImageRef image,
                                 ThumbnailCapturer::SourceId source_id)>;
using ErrorCallback = base::RepeatingClosure;

API_AVAILABLE(macos(13.2))
@interface SCKStreamDelegateAndOutput
    : NSObject <SCStreamDelegate, SCStreamOutput>

- (instancetype)initWithSampleCallback:(SampleCallback)sampleCallback
                         errorCallback:(ErrorCallback)errorCallback
                              sourceId:(ThumbnailCapturer::SourceId)sourceId;
@end

@implementation SCKStreamDelegateAndOutput {
  SampleCallback _sampleCallback;
  ErrorCallback _errorCallback;
  ThumbnailCapturer::SourceId _sourceId;
}

- (instancetype)initWithSampleCallback:(SampleCallback)sampleCallback
                         errorCallback:(ErrorCallback)errorCallback
                              sourceId:(ThumbnailCapturer::SourceId)sourceId {
  if (self = [super init]) {
    _sampleCallback = sampleCallback;
    _errorCallback = errorCallback;
    _sourceId = sourceId;
  }
  return self;
}

+ (CGRect)cropRegionFromSampleBuffer:(CMSampleBufferRef)sampleBuffer {
  // Read out the content region from the sample buffer attachment metadata and
  // use as crop region. The content region is determined from the attachments
  // SCStreamFrameInfoContentRect and SCStreamFrameInfoScaleFactor.

  CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(
      sampleBuffer, /*createIfNecessary=*/false);
  if (!attachmentsArray || CFArrayGetCount(attachmentsArray) <= 0) {
    return {};
  }

  CFDictionaryRef attachment = base::apple::CFCast<CFDictionaryRef>(
      CFArrayGetValueAtIndex(attachmentsArray, 0));
  if (!attachment) {
    return {};
  }

  CFDictionaryRef contentRectValue = base::apple::CFCast<CFDictionaryRef>(
      CFDictionaryGetValue(attachment, base::apple::NSToCFPtrCast(
                                           SCStreamFrameInfoContentRect)));
  CFNumberRef scaleFactorValue = base::apple::CFCast<CFNumberRef>(
      CFDictionaryGetValue(attachment, base::apple::NSToCFPtrCast(
                                           SCStreamFrameInfoScaleFactor)));
  if (!contentRectValue || !scaleFactorValue) {
    return {};
  }

  CGRect contentRect = {};
  float scaleFactor = 1.0f;
  if (!CGRectMakeWithDictionaryRepresentation(contentRectValue, &contentRect) ||
      !CFNumberGetValue(scaleFactorValue, kCFNumberFloatType, &scaleFactor)) {
    return {};
  }

  contentRect.size.width *= scaleFactor;
  contentRect.size.height *= scaleFactor;
  return contentRect;
}

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  if (!pixelBuffer) {
    return;
  }
  CGImageRef cgImage = nil;
  auto result = VTCreateCGImageFromCVPixelBuffer(pixelBuffer, nil, &cgImage);

  if (result != 0) {
    return;
  }

  // Avoid having black regions on the sides by cropping the image to the
  // content region.
  CGRect cropRegion =
      [SCKStreamDelegateAndOutput cropRegionFromSampleBuffer:sampleBuffer];
  if (CGRectIsEmpty(cropRegion)) {
    return;
  }

  CGImageRef croppedImage = CGImageCreateWithImageInRect(cgImage, cropRegion);
  CGImageRelease(cgImage);
  _sampleCallback.Run(croppedImage, _sourceId);
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
  _errorCallback.Run();
}

@end

namespace {

BASE_FEATURE(kScreenCaptureKitStreamPickerSonoma,
             "ScreenCaptureKitStreamPickerSonoma",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kScreenCaptureKitStreamPickerVentura,
             "ScreenCaptureKitStreamPickerVentura",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The enable/disable property of this feature has no impact. The feature is
// used solely to pass on the parameters below.
BASE_FEATURE(kThumbnailCapturerMac,
             "ThumbnailCapturerMac",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Default max frame rate that is used when capturing thumbnails. This is per
// source so the combined frame rate can be higher if there are multiple
// sources.
const base::FeatureParam<int> kThumbnailCapturerMacMaxFrameRate{
    &kThumbnailCapturerMac, "max_frame_rate", 1};

// Refresh interval that is used to query for the list of shareable content.
const base::FeatureParam<base::TimeDelta>
    kThumbnailCapturerMacRefreshTimerInterval{&kThumbnailCapturerMac,
                                              "refresh_timer_interval",
                                              base::Milliseconds(250)};

// The sort mode controls the order of the source list that is returned from
// GetSourceList().
enum class SortMode {
  // No extra sorting, same order as returned by SCShareableContent.
  kNone = 0,
  // Same order as returned by CGWindowListCopyWindowInfo().
  kCGWindowList = 1,
  // Static order where new windows are put last in the list.
  kNewWindowsLast = 2
};
const base::FeatureParam<SortMode>::Option sort_mode_options[] = {
    {SortMode::kNone, "none"},
    {SortMode::kCGWindowList, "cg_window_list"},
    {SortMode::kNewWindowsLast, "new_windows_last"},
};
const base::FeatureParam<SortMode> kThumbnailCapturerMacSortMode{
    &kThumbnailCapturerMac, "sort_mode", SortMode::kCGWindowList,
    &sort_mode_options};

// The minimum window size that is still considered to be a shareable window.
// Windows with smaller height or widht are filtered out.
const base::FeatureParam<int> kThumbnailCapturerMacMinWindowSize{
    &kThumbnailCapturerMac, "min_window_size", 40};

bool API_AVAILABLE(macos(12.3))
    IsWindowFullscreen(SCWindow* window, NSArray<SCDisplay*>* displays) {
  for (SCDisplay* display : displays) {
    if (CGRectEqualToRect(window.frame, display.frame)) {
      return true;
    }
  }
  return false;
}

SCWindow* API_AVAILABLE(macos(12.3))
    FindWindow(NSArray<SCWindow*>* array, CGWindowID window_id) {
  for (SCWindow* window in array) {
    if ([window windowID] == window_id) {
      return window;
    }
  }
  return nil;
}

CGWindowID GetWindowId(CFArrayRef window_array, CFIndex index) {
  CFDictionaryRef window_ref = reinterpret_cast<CFDictionaryRef>(
      CFArrayGetValueAtIndex(window_array, index));
  if (!window_ref) {
    return kCGNullWindowID;
  }

  CFNumberRef window_id_ref = reinterpret_cast<CFNumberRef>(
      CFDictionaryGetValue(window_ref, kCGWindowNumber));
  if (!window_id_ref) {
    return kCGNullWindowID;
  }
  CGWindowID window_id;
  if (!CFNumberGetValue(window_id_ref, kCFNumberIntType, &window_id)) {
    return kCGNullWindowID;
  }
  return window_id;
}

class API_AVAILABLE(macos(13.2)) ThumbnailCapturerMac
    : public ThumbnailCapturer {
 public:
  ThumbnailCapturerMac();
  ~ThumbnailCapturerMac() override{};

  void Start(Consumer* callback) override;

  FrameDeliveryMethod GetFrameDeliveryMethod() const override {
    return FrameDeliveryMethod::kMultipleSourcesRecurrent;
  }

  // Sets the maximum frame rate for the thumbnail streams. This should be
  // called before the call to Start because any stream that is already created
  // will not be affected by the change to max frame rate.
  void SetMaxFrameRate(uint32_t max_frame_rate) override;

  bool GetSourceList(SourceList* sources) override;

  void SelectSources(const std::vector<SourceId>& ids,
                     gfx::Size thumbnail_size) override;

 private:
  struct StreamAndDelegate {
    SCStream* __strong stream;
    SCKStreamDelegateAndOutput* __strong delegate;
  };

  void UpdateWindowsList();
  void OnRecurrentShareableContent(SCShareableContent* content);

  void UpdateShareableWindows(NSArray<SCWindow*>* content_windows);

  // Returns the supplied list of windows sorted to have the same order as
  // returned from CGWindowListCopyWindowInfo.
  NSArray<SCWindow*>* SortOrderByCGWindowList(
      NSArray<SCWindow*>* current_windows) const;

  // Returns the supplied list of windows sorted so that new windows (i.e., not
  // currently in shareable_windows_) are put last in the list.
  NSArray<SCWindow*>* SortOrderByNewWindowsLast(
      NSArray<SCWindow*>* current_windows) const;

  bool IsShareable(SCWindow* window) const;
  NSArray<SCWindow*>* FilterOutUnshareable(NSArray<SCWindow*>* windows);
  void RemoveInactiveStreams();
  void OnCapturedFrame(CGImageRef image, SourceId source_id);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const SortMode sort_mode_;
  int max_frame_rate_;
  const int minimum_window_size_;
  raw_ptr<Consumer> consumer_;

  // A cache of the shareable windows and shareable displays. sharable_windows_
  // is used to produce the source list. shareable_displays_ is used to
  // determine if a window is fullscreen or not. Both are updated continuously
  // by the refresh_timer_.
  NSArray<SCWindow*>* __strong shareable_windows_;
  NSArray<SCDisplay*>* __strong shareable_displays_;

  std::unordered_map<SourceId, StreamAndDelegate> streams_;
  base::RepeatingTimer refresh_timer_;
  base::WeakPtrFactory<ThumbnailCapturerMac> weak_factory_{this};
};

ThumbnailCapturerMac::ThumbnailCapturerMac()
    : sort_mode_(kThumbnailCapturerMacSortMode.Get()),
      max_frame_rate_(kThumbnailCapturerMacMaxFrameRate.Get()),
      minimum_window_size_(kThumbnailCapturerMacMinWindowSize.Get()),
      shareable_windows_([[NSArray<SCWindow*> alloc] init]) {}

void ThumbnailCapturerMac::Start(Consumer* consumer) {
  consumer_ = consumer;
  task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  UpdateWindowsList();
  // Start a timer that periodically update the list of sharable windows.
  refresh_timer_.Start(FROM_HERE,
                       kThumbnailCapturerMacRefreshTimerInterval.Get(), this,
                       &ThumbnailCapturerMac::UpdateWindowsList);
}

void ThumbnailCapturerMac::SetMaxFrameRate(uint32_t max_frame_rate) {
  max_frame_rate_ = max_frame_rate;
}

bool ThumbnailCapturerMac::GetSourceList(SourceList* sources) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  sources->clear();

  // Discover how many windows are associated with each application,
  // so as to use this as part of the set of conditions for which
  // windows are valid sources.
  std::unordered_map<pid_t, size_t> application_to_window_count;
  for (SCWindow* window in shareable_windows_) {
    const pid_t pid = window.owningApplication.processID;
    if (!base::Contains(application_to_window_count, pid)) {
      application_to_window_count[pid] = 1;
    } else {
      ++application_to_window_count[pid];
    }
  }

  // Add relevant sources.
  for (SCWindow* window in shareable_windows_) {
    // Skip windows with empty titles, unless they are their app's only window
    // or fullscreen.
    const pid_t pid = window.owningApplication.processID;
    bool is_title_empty = [window.title length] <= 0;
    if (is_title_empty && application_to_window_count.at(pid) > 1 &&
        !IsWindowFullscreen(window, shareable_displays_)) {
      continue;
    }

    sources->push_back(ThumbnailCapturer::Source{
        window.windowID,
        base::SysNSStringToUTF8(is_title_empty
                                    ? window.owningApplication.applicationName
                                    : window.title)});
  }

  return true;
}

void ThumbnailCapturerMac::UpdateWindowsList() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto content_callback = base::BindPostTask(
      task_runner_,
      base::BindRepeating(&ThumbnailCapturerMac::OnRecurrentShareableContent,
                          weak_factory_.GetWeakPtr()));

  auto handler = ^(SCShareableContent* content, NSError* error) {
    content_callback.Run(content);
  };

  // Exclude desktop windows (e.g., background image and deskktop icons) and
  // windows that are not on screen (e.g., minimized and behind fullscreen
  // windows).
  [SCShareableContent getShareableContentExcludingDesktopWindows:true
                                             onScreenWindowsOnly:true
                                               completionHandler:handler];
}

void ThumbnailCapturerMac::OnRecurrentShareableContent(
    SCShareableContent* content) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!content) {
    return;
  }

  shareable_displays_ = [content displays];
  UpdateShareableWindows([content windows]);

  // TODO(https://crbug.com/1471931): Only call update if the list is changed:
  // windows opened/closed, order of the list, and title.
  consumer_->OnSourceListUpdated();
}

void ThumbnailCapturerMac::UpdateShareableWindows(
    NSArray<SCWindow*>* content_windows) {
  // Narrow down the list to shareable windows.
  content_windows = FilterOutUnshareable(content_windows);

  // Update shareable_streams_ from current_windows.
  switch (sort_mode_) {
    case SortMode::kNone:
      shareable_windows_ = content_windows;
      break;
    case SortMode::kCGWindowList:
      shareable_windows_ = SortOrderByCGWindowList(content_windows);
      break;
    case SortMode::kNewWindowsLast:
      shareable_windows_ = SortOrderByNewWindowsLast(content_windows);
      break;
  }

  RemoveInactiveStreams();
}

NSArray<SCWindow*>* ThumbnailCapturerMac::SortOrderByCGWindowList(
    NSArray<SCWindow*>* current_windows) const {
  CHECK_EQ(sort_mode_, SortMode::kCGWindowList);

  // Only get on screen, non-desktop windows.
  // According to
  // https://developer.apple.com/documentation/coregraphics/cgwindowlistoption/1454105-optiononscreenonly
  // when kCGWindowListOptionOnScreenOnly is used, the order of windows are
  // in decreasing z-order.
  CFArrayRef window_array = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID);
  if (!window_array) {
    DVLOG(2) << "Cannot sort list, nothing returned from "
                "CGWindowListCopyWindowInfo.";
    return current_windows;
  }

  // Sort `current_windows` to match the order returned
  // by CGWindowListCopyWindowInfo.
  // The windowID is the key matching entries in these containers.
  NSMutableArray<SCWindow*>* sorted_windows = [[NSMutableArray alloc] init];
  CFIndex count = CFArrayGetCount(window_array);
  for (CFIndex i = 0; i < count; i++) {
    CGWindowID window_id = GetWindowId(window_array, i);
    SCWindow* window = FindWindow(current_windows, window_id);
    if (window) {
      [sorted_windows addObject:window];
    }
  }
  return sorted_windows;
}

NSArray<SCWindow*>* ThumbnailCapturerMac::SortOrderByNewWindowsLast(
    NSArray<SCWindow*>* current_windows) const {
  CHECK_EQ(sort_mode_, SortMode::kNewWindowsLast);

  // Prepare to segment the list of new window as pre-existing / newly-added.
  NSMutableArray<SCWindow*>* existing_windows = [[NSMutableArray alloc] init];
  NSMutableArray<SCWindow*>* added_windows = [[NSMutableArray alloc] init];

  // Iterate over the windows from last time and ensure that all of them
  // which are still relevant, are maintained in their original order.
  for (SCWindow* window in shareable_windows_) {
    SCWindow* current_window = FindWindow(current_windows, window.windowID);
    if (current_window) {
      // Please note that current_window may not be equal to the previous window
      // despite that they have the same WindowID if for example the title has
      // changed.
      [existing_windows addObject:current_window];
    }
  }

  // All other windows in `current_windows` are new by definition.
  for (SCWindow* window in current_windows) {
    if (!FindWindow(existing_windows, window.windowID)) {
      [added_windows addObject:window];
    }
  }

  return [existing_windows arrayByAddingObjectsFromArray:added_windows];
}

bool ThumbnailCapturerMac::IsShareable(SCWindow* window) const {
  // Always exclude windows from the source list based on the following
  // conditions:
  // 1. Exclude windows with layer!=0 (menu, dock).
  // 2. Exclude small windows with either height or width less than the minimum.
  //    Such windows are generally of no interest to the user, cluttering the
  //    thumbnail picker and serving only as a footgun for the user.
  //    For example, on macOS 14, each window that is captured has an indicator
  //    that the window is being captured. This indicator is a window itself,
  //    but is of no use for the user.
  return window.windowLayer == 0 &&
         window.frame.size.height >= minimum_window_size_ &&
         window.frame.size.width >= minimum_window_size_;
}

NSArray<SCWindow*>* ThumbnailCapturerMac::FilterOutUnshareable(
    NSArray<SCWindow*>* windows) {
  NSMutableArray<SCWindow*>* result = [[NSMutableArray<SCWindow*> alloc] init];
  for (SCWindow* window in windows) {
    if (IsShareable(window)) {
      [result addObject:window];
    }
  }
  return result;
}

void ThumbnailCapturerMac::RemoveInactiveStreams() {
  // Remove all streams for windows that are not active anymore. New streams are
  // created once the consumer calls SelectSources().
  for (auto it = streams_.begin(); it != streams_.end();) {
    if (!FindWindow(shareable_windows_, it->first)) {
      it = streams_.erase(it);
    } else {
      ++it;
    }
  }
}

void ThumbnailCapturerMac::SelectSources(const std::vector<SourceId>& ids,
                                         gfx::Size thumbnail_size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Create SCStreamConfiguration.
  SCStreamConfiguration* __strong config = [[SCStreamConfiguration alloc] init];
  config.width = thumbnail_size.width();
  config.height = thumbnail_size.height();
  config.scalesToFit = YES;
  config.showsCursor = NO;
  config.minimumFrameInterval = CMTimeMake(
      media::kFrameRatePrecision,
      static_cast<int>(max_frame_rate_ * media::kFrameRatePrecision));

  SampleCallback sample_callback = base::BindPostTask(
      task_runner_, base::BindRepeating(&ThumbnailCapturerMac::OnCapturedFrame,
                                        weak_factory_.GetWeakPtr()));
  ErrorCallback error_callback = base::DoNothing();

  // Create a stream for any source that doesn't have a stream.
  std::unordered_set<SourceId> selected_sources;
  for (SourceId id : ids) {
    selected_sources.insert(id);
    if (streams_.find(id) != streams_.end()) {
      continue;
    }
    // Find the corresponding SCWindow in the list.
    SCWindow* selected_window = nil;
    for (SCWindow* window in shareable_windows_) {
      if (window.windowID == id) {
        selected_window = window;
        break;
      }
    }

    if (!selected_window) {
      continue;
    }
    SCContentFilter* __strong filter = [[SCContentFilter alloc]
        initWithDesktopIndependentWindow:selected_window];

    SCKStreamDelegateAndOutput* __strong delegate =
        [[SCKStreamDelegateAndOutput alloc]
            initWithSampleCallback:sample_callback
                     errorCallback:error_callback
                          sourceId:selected_window.windowID];

    // Create and start stream.
    SCStream* __strong stream = [[SCStream alloc] initWithFilter:filter
                                                   configuration:config
                                                        delegate:delegate];

    NSError* error = nil;
    bool add_stream_output_result =
        [stream addStreamOutput:delegate
                           type:SCStreamOutputTypeScreen
             sampleHandlerQueue:dispatch_get_main_queue()
                          error:&error];
    if (error || !add_stream_output_result) {
      DVLOG(2) << "Something went wrong while adding stream output";
      continue;
    }

    auto handler = ^(NSError* e) {
      if (e) {
        DVLOG(2) << "Error while starting the capturer.";
      }
    };
    [stream startCaptureWithCompletionHandler:handler];

    // Keep track of the stream and delegate.
    streams_[id] = StreamAndDelegate{stream, delegate};
  }

  // Remove any stream that is not in the list of selected sources anymore.
  for (auto it = streams_.begin(); it != streams_.end();) {
    if (selected_sources.find(it->first) == selected_sources.end()) {
      it = streams_.erase(it);
    } else {
      ++it;
    }
  }
}

void ThumbnailCapturerMac::OnCapturedFrame(
    CGImageRef image,
    ThumbnailCapturer::SourceId source_id) {
  if (!image) {
    return;
  }

  // The image has been captured, pass it on to the consumer as a DesktopFrame.
  rtc::ScopedCFTypeRef<CGImageRef> cg_image(image);
  std::unique_ptr<webrtc::DesktopFrame> frame =
      webrtc::CreateDesktopFrameFromCGImage(cg_image);
  consumer_->OnRecurrentCaptureResult(Result::SUCCESS, std::move(frame),
                                      source_id);
}

}  // namespace

bool ShouldUseThumbnailCapturerMac() {
  if (@available(macOS 14.0, *)) {
    return base::FeatureList::IsEnabled(kScreenCaptureKitStreamPickerSonoma);
  }
  if (@available(macOS 13.2, *)) {
    return base::FeatureList::IsEnabled(kScreenCaptureKitStreamPickerVentura);
  }
  return false;
}

// Creates a ThumbnailCapturerMac object. Must only be called is
// ShouldUseThumbnailCapturerMac() returns true.
std::unique_ptr<ThumbnailCapturer> CreateThumbnailCapturerMac() {
  CHECK(ShouldUseThumbnailCapturerMac());
  if (@available(macOS 13.2, *)) {
    return std::make_unique<ThumbnailCapturerMac>();
  }
  NOTREACHED_NORETURN();
}
