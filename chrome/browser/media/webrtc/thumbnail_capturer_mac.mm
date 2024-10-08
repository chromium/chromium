// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/thumbnail_capturer_mac.h"

#include <CoreGraphics/CoreGraphics.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <VideoToolbox/VideoToolbox.h>

#include <cmath>
#include <deque>
#include <optional>
#include <unordered_map>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/timer/timer.h"
#include "chrome/browser/media/webrtc/delegated_source_list_capturer.h"
#include "chrome/browser/media/webrtc/desktop_capturer_wrapper.h"
#include "chrome/browser/media/webrtc/desktop_media_list_base.h"
#include "media/base/media_switches.h"
#include "media/capture/video_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/mac/desktop_frame_utils.h"

using SampleCallback =
    base::RepeatingCallback<void(base::apple::ScopedCFTypeRef<CGImageRef> image,
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
  base::apple::ScopedCFTypeRef<CGImageRef> cgImage;
  auto result = VTCreateCGImageFromCVPixelBuffer(pixelBuffer, nil,
                                                 cgImage.InitializeInto());

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

  base::apple::ScopedCFTypeRef<CGImageRef> croppedImage(
      CGImageCreateWithImageInRect(cgImage.get(), cropRegion));
  _sampleCallback.Run(croppedImage, _sourceId);
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
  _errorCallback.Run();
}

@end

namespace {

// These features enable the use of ScreenCaptureKit to produce thumbnails in
// the getDisplayMedia picker. The `kScreenCaptureKitStreamPicker*` features are
// used to produce thumbnails of specific windows, while the
// `kScreenCaptureKitPickerScreen` feature is used to produce thumbnails of the
// entire screen. This is distinct from `kUseSCContentSharingPicker`, which uses
// the native macOS picker.
BASE_FEATURE(kScreenCaptureKitStreamPickerSonoma,
             "ScreenCaptureKitStreamPickerSonoma",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kScreenCaptureKitStreamPickerVentura,
             "ScreenCaptureKitStreamPickerVentura",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kScreenCaptureKitPickerScreen,
             "ScreenCaptureKitPickerScreen",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// The capture mode controls how the thumbnails are captured.
enum class CaptureMode {
  // Create an SCStream for each selected source. In this mode frames are pushed
  // by the OS at the specified maximum frame rate.
  kSCStream = 0,
  // Use SCScreenshotManager to capture frames. In this mode a timer is used to
  // periodically capture the selected windows in a pull-based fashion. Please
  // note that this mode is only available in macOS 14.0 and later.
  kSCScreenshotManager = 1
};
const base::FeatureParam<CaptureMode>::Option capture_mode_options[] = {
    {CaptureMode::kSCStream, "sc_stream"},
    {CaptureMode::kSCScreenshotManager, "sc_screenshot_manager"},
};
const base::FeatureParam<CaptureMode> kThumbnailCapturerMacCaptureMode{
    &kThumbnailCapturerMac, "capture_mode", CaptureMode::kSCScreenshotManager,
    &capture_mode_options};

CaptureMode GetCaptureModeFeatureParam() {
  if (@available(macOS 14.0, *)) {
    return kThumbnailCapturerMacCaptureMode.Get();
  }
  return CaptureMode::kSCStream;
}

content::DesktopMediaID::Type ConvertToDesktopMediaIDType(
    DesktopMediaList::Type type) {
  switch (type) {
    case DesktopMediaList::Type::kScreen:
      return content::DesktopMediaID::Type::TYPE_SCREEN;
    case DesktopMediaList::Type::kWindow:
      return content::DesktopMediaID::Type::TYPE_WINDOW;
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      return content::DesktopMediaID::Type::TYPE_WEB_CONTENTS;
    case DesktopMediaList::Type::kNone:
      break;
  }
  NOTREACHED_NORETURN();
}

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

// Controls the maximum number of sources that are captured during each capture
// cycle if the capture mode is set to kSCScreenshotManager. By having a limit
// and cycling through what windows are captured we get a graceful degradation.
const base::FeatureParam<int> kThumbnailCapturerMacMaxSourcesPerCycles{
    &kThumbnailCapturerMac, "max_sources_per_cycles", 25};

bool API_AVAILABLE(macos(12.3))
    IsWindowFullscreen(SCWindow* window, NSArray<SCDisplay*>* displays) {
  for (SCDisplay* display in displays) {
    if (CGRectEqualToRect(window.frame, display.frame)) {
      return true;
    }
  }
  return false;
}

SCDisplay* API_AVAILABLE(macos(12.3))
    FindDisplay(NSArray<SCDisplay*>* array, CGDirectDisplayID display_id) {
  for (SCDisplay* display in array) {
    if ([display displayID] == display_id) {
      return display;
    }
  }
  return nil;
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

class API_AVAILABLE(macos(12.3)) ScreenshotManagerCapturer {
 public:
  using GetShareableDisplayCallback = base::RepeatingCallback<SCDisplay*(
      ThumbnailCapturer::SourceId source_id)>;
  using GetShareableWindowCallback =
      base::RepeatingCallback<SCWindow*(ThumbnailCapturer::SourceId source_id)>;

  ScreenshotManagerCapturer(
      DesktopMediaList::Type type,
      int max_frame_rate,
      GetShareableDisplayCallback get_shareable_display_callback,
      GetShareableWindowCallback get_shareable_window_callback,
      SampleCallback sample_callback);
  void SelectSources(const std::vector<ThumbnailCapturer::SourceId>& ids,
                     gfx::Size thumbnail_size);

 private:
  void API_AVAILABLE(macos(14.0)) OnRecurrentCaptureTimer();
  void API_AVAILABLE(macos(14.0))
      OnCapturedFrame(base::apple::ScopedCFTypeRef<CGImageRef> cg_image,
                      ThumbnailCapturer::SourceId source_id);
  void API_AVAILABLE(macos(14.0))
      CaptureSource(ThumbnailCapturer::SourceId source_id);

  void API_AVAILABLE(macos(14.0))
      SCScreenshotCaptureSource(SCContentFilter* filter,
                                CGRect frame,
                                ThumbnailCapturer::SourceId source_id);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The type of source, kScreen and kWindow are supported.
  DesktopMediaList::Type type_;

  // Callback to retrieve an SCDisplay* based on source ID.
  GetShareableDisplayCallback get_shareable_display_callback_;

  // Callback to retrieve an SCWindow* based on source ID.
  GetShareableWindowCallback get_shareable_window_callback_;

  // Callback that is used whenever a thumbnail is captured.
  SampleCallback sample_callback_;

  // The maximum number of sources that can be captured in each capture cycle.
  // This is also the maximum number of capture calls that can be in-flight
  // simultaneously. We have a limit here to not spawn hundreds of capturers at
  // the same time since this could degrade the system performance.
  const size_t max_sources_per_cycle_;

  // The number of calls to SCScreenshotManager for which we have not yet
  // received the corresponding callback with a captured frame (or potentially
  // an error).
  size_t capture_calls_in_flight_ = 0;

  // The selected sources, this is used to determine if a selected source was
  // not selected before and give priority to the source in this case.
  std::vector<ThumbnailCapturer::SourceId> selected_sources_;

  // The capture queue is used to maintain a list of all selected sources and
  // keep track of what source should be captured next in the case that too many
  // sources are selected and we cannot capture all sources in each capture
  // cycle.
  std::deque<ThumbnailCapturer::SourceId> capture_queue_;

  gfx::Size thumbnail_size_ = kDefaultThumbnailSize;

  base::RepeatingTimer capture_frame_timer_;

  base::WeakPtrFactory<ScreenshotManagerCapturer> weak_factory_{this};
};

ScreenshotManagerCapturer::ScreenshotManagerCapturer(
    DesktopMediaList::Type type,
    int max_frame_rate,
    GetShareableDisplayCallback get_shareable_display_callback,
    GetShareableWindowCallback get_shareable_window_callback,
    SampleCallback sample_callback)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      type_(type),
      get_shareable_display_callback_(get_shareable_display_callback),
      get_shareable_window_callback_(get_shareable_window_callback),
      sample_callback_(sample_callback),
      max_sources_per_cycle_(kThumbnailCapturerMacMaxSourcesPerCycles.Get()) {
  if (@available(macOS 14.0, *)) {
    capture_frame_timer_.Start(
        FROM_HERE, base::Milliseconds(1000.0 / max_frame_rate), this,
        &ScreenshotManagerCapturer::OnRecurrentCaptureTimer);
  }
}

void ScreenshotManagerCapturer::SelectSources(
    const std::vector<ThumbnailCapturer::SourceId>& ids,
    gfx::Size thumbnail_size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  thumbnail_size_ = thumbnail_size;

  // The iteration is in reverse order so that the sources
  // first in the list are captured first. This way we make sure that the first
  // thumbnails in the view are captured first.
  bool new_sources_added = false;
  for (ThumbnailCapturer::SourceId source_id : base::Reversed(ids)) {
    if (!base::Contains(selected_sources_, source_id)) {
      capture_queue_.push_front(source_id);
      new_sources_added = true;
    }
  }

  selected_sources_ = ids;
  if (new_sources_added) {
    // Run the capture code immediately to avoid a short period with empty
    // thumbnails at the top of the list. This is especially useful in the first
    // call to SelectSources().
    if (@available(macOS 14.0, *)) {
      OnRecurrentCaptureTimer();
      capture_frame_timer_.Reset();
    }
  }
}

void ScreenshotManagerCapturer::OnRecurrentCaptureTimer() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (capture_queue_.empty()) {
    return;
  }

  // Take source ids from the top of the queue and capture the corresponding
  // window if it is still selected and exists in the list of shareable windows.
  CHECK_LE(capture_calls_in_flight_, max_sources_per_cycle_);
  size_t sources_to_capture = std::min(
      capture_queue_.size(), max_sources_per_cycle_ - capture_calls_in_flight_);
  for (size_t i = 0; i < sources_to_capture; ++i) {
    ThumbnailCapturer::SourceId source_id = capture_queue_.front();
    capture_queue_.pop_front();
    if (!base::Contains(selected_sources_, source_id)) {
      continue;
    }
    CaptureSource(source_id);
  }
}

void ScreenshotManagerCapturer::CaptureSource(
    ThumbnailCapturer::SourceId source_id) {
  switch (type_) {
    case DesktopMediaList::Type::kScreen: {
      // Find the corresponding SCDisplay in the list.
      SCDisplay* selected_display =
          get_shareable_display_callback_.Run(source_id);
      if (!selected_display) {
        return;
      }

      SCContentFilter* filter =
          [[SCContentFilter alloc] initWithDisplay:selected_display
                                  excludingWindows:@[]];
      SCScreenshotCaptureSource(filter, [selected_display frame],
                                [selected_display displayID]);
      break;
    }
    case DesktopMediaList::Type::kWindow: {
      // Find the corresponding SCWindow in the list.
      SCWindow* selected_window = get_shareable_window_callback_.Run(source_id);
      if (!selected_window) {
        return;
      }

      SCContentFilter* filter = [[SCContentFilter alloc]
          initWithDesktopIndependentWindow:selected_window];
      SCScreenshotCaptureSource(filter, [selected_window frame],
                                [selected_window windowID]);
      break;
    }
    case DesktopMediaList::Type::kNone:
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      NOTREACHED();
  }
}

void ScreenshotManagerCapturer::OnCapturedFrame(
    base::apple::ScopedCFTypeRef<CGImageRef> cg_image,
    ThumbnailCapturer::SourceId source_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Schedule a new capture of this window since we got a callback.
  CHECK_GT(capture_calls_in_flight_, 0u);
  --capture_calls_in_flight_;
  capture_queue_.push_back(source_id);

  if (cg_image) {
    sample_callback_.Run(cg_image, source_id);
  }
}

void ScreenshotManagerCapturer::SCScreenshotCaptureSource(
    SCContentFilter* filter,
    CGRect frame,
    ThumbnailCapturer::SourceId source_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Create SCStreamConfiguration.
  SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
  config.scalesToFit = YES;
  config.showsCursor = NO;

  // Avoid black regions in the captured frame by setting width and height to
  // the same aspect ratio as the window.
  float thumbnail_aspect_ratio = static_cast<float>(thumbnail_size_.width()) /
                                 static_cast<float>(thumbnail_size_.height());
  float source_aspect_ratio = frame.size.width / frame.size.height;
  if (source_aspect_ratio > thumbnail_aspect_ratio) {
    config.width = thumbnail_size_.width();
    config.height = std::round(thumbnail_size_.width() / source_aspect_ratio);
  } else {
    config.height = thumbnail_size_.height();
    config.width = std::round(thumbnail_size_.height() * source_aspect_ratio);
  }

  auto captured_frame_callback = base::BindPostTask(
      task_runner_,
      base::BindRepeating(&ScreenshotManagerCapturer::OnCapturedFrame,
                          weak_factory_.GetWeakPtr()));

  auto handler = ^(CGImageRef sampleBuffer, NSError* error) {
    base::apple::ScopedCFTypeRef<CGImageRef> scopedImage(
        error ? nil : sampleBuffer, base::scoped_policy::RETAIN);
    captured_frame_callback.Run(scopedImage, source_id);
  };
  ++capture_calls_in_flight_;
  [SCScreenshotManager captureImageWithFilter:filter
                                configuration:config
                            completionHandler:handler];
}

class API_AVAILABLE(macos(13.2)) ThumbnailCapturerMac
    : public ThumbnailCapturer {
 public:
  explicit ThumbnailCapturerMac(DesktopMediaList::Type type);
  ~ThumbnailCapturerMac() override;

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

  void GetDisplaySourceList(SourceList* sources) const;
  void GetWindowSourceList(SourceList* sources) const;

  void UpdateShareableWindows(NSArray<SCWindow*>* content_windows);
  SCDisplay* GetShareableDisplay(SourceId source_id) const;
  SCWindow* GetShareableWindow(SourceId source_id) const;

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
  void OnCapturedFrame(base::apple::ScopedCFTypeRef<CGImageRef> image,
                       ThumbnailCapturer::SourceId source_id);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DesktopMediaList::Type type_;
  const CaptureMode capture_mode_;
  const SortMode sort_mode_;
  int max_frame_rate_;
  const int minimum_window_size_;
  raw_ptr<Consumer> consumer_;
  int shareable_content_callbacks_ = 0;
  int shareable_content_errors_ = 0;

  // A cache of the shareable windows and shareable displays. sharable_windows_
  // is used to produce the source list. shareable_displays_ is used to
  // determine if a window is fullscreen or not. Both are updated continuously
  // by the refresh_timer_.
  NSArray<SCWindow*>* __strong shareable_windows_;
  NSArray<SCDisplay*>* __strong shareable_displays_;

  base::flat_map<SourceId, StreamAndDelegate> streams_;
  base::RepeatingTimer refresh_timer_;

  std::unique_ptr<ScreenshotManagerCapturer> screenshot_manager_capturer_;

  base::WeakPtrFactory<ThumbnailCapturerMac> weak_factory_{this};
};

ThumbnailCapturerMac::ThumbnailCapturerMac(DesktopMediaList::Type type)
    : type_(type),
      capture_mode_(type_ == DesktopMediaList::Type::kScreen
                        ? CaptureMode::kSCScreenshotManager
                        : GetCaptureModeFeatureParam()),
      sort_mode_(kThumbnailCapturerMacSortMode.Get()),
      max_frame_rate_(kThumbnailCapturerMacMaxFrameRate.Get()),
      minimum_window_size_(kThumbnailCapturerMacMinWindowSize.Get()),
      shareable_windows_([[NSArray<SCWindow*> alloc] init]) {
  CHECK(type_ == DesktopMediaList::Type::kWindow ||
        type_ == DesktopMediaList::Type::kScreen);
}

ThumbnailCapturerMac::~ThumbnailCapturerMac() {
  if (shareable_content_callbacks_ > 0) {
    // Round upwards so that a single callback with error will show up in the
    // histogram as greater than 0%.
    int error_percentage = static_cast<int>(std::ceil(
        (100.0 * shareable_content_errors_) / shareable_content_callbacks_));
    base::UmaHistogramPercentage(
        "Media.ThumbnailCapturerMac.ShareableContentErrorPercentage",
        error_percentage);
  }
}

void ThumbnailCapturerMac::Start(Consumer* consumer) {
  consumer_ = consumer;
  task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  UpdateWindowsList();
  // Start a timer that periodically update the list of sharable windows.
  refresh_timer_.Start(FROM_HERE,
                       kThumbnailCapturerMacRefreshTimerInterval.Get(), this,
                       &ThumbnailCapturerMac::UpdateWindowsList);

  if (capture_mode_ == CaptureMode::kSCScreenshotManager) {
    CHECK(!screenshot_manager_capturer_);
    // Unretained is safe because `screenshot_manager_capturer_ ` is owned by
    // `this`, and hence has a shorter lifetime than `this`.
    screenshot_manager_capturer_ = std::make_unique<ScreenshotManagerCapturer>(
        type_, max_frame_rate_,
        base::BindRepeating(&ThumbnailCapturerMac::GetShareableDisplay,
                            base::Unretained(this)),
        base::BindRepeating(&ThumbnailCapturerMac::GetShareableWindow,
                            base::Unretained(this)),
        base::BindRepeating(&ThumbnailCapturerMac::OnCapturedFrame,
                            weak_factory_.GetWeakPtr()));
  }
}

void ThumbnailCapturerMac::SetMaxFrameRate(uint32_t max_frame_rate) {
  max_frame_rate_ = max_frame_rate;
}

bool ThumbnailCapturerMac::GetSourceList(SourceList* sources) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  sources->clear();

  switch (type_) {
    case DesktopMediaList::Type::kScreen:
      GetDisplaySourceList(sources);
      break;
    case DesktopMediaList::Type::kWindow:
      GetWindowSourceList(sources);
      break;
    case DesktopMediaList::Type::kNone:
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      NOTREACHED();
  }

  return true;
}

void ThumbnailCapturerMac::GetDisplaySourceList(SourceList* sources) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Add relevant sources.
  for (SCDisplay* display in shareable_displays_) {
    sources->push_back(
        ThumbnailCapturer::Source{display.displayID, std::string()});
  }
}

void ThumbnailCapturerMac::GetWindowSourceList(SourceList* sources) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
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
}

void ThumbnailCapturerMac::UpdateWindowsList() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto content_callback = base::BindPostTask(
      task_runner_,
      base::BindRepeating(&ThumbnailCapturerMac::OnRecurrentShareableContent,
                          weak_factory_.GetWeakPtr()));

  auto handler = ^(SCShareableContent* content, NSError* error) {
    content_callback.Run(error ? nil : content);
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

  ++shareable_content_callbacks_;
  if (!content) {
    DVLOG(2) << "Could not get shareable content.";
    ++shareable_content_errors_;
    return;
  }

  shareable_displays_ = [content displays];
  UpdateShareableWindows([content windows]);

  // TODO(crbug.com/40278456): Only call update if the list is changed:
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

SCDisplay* ThumbnailCapturerMac::GetShareableDisplay(SourceId source_id) const {
  return FindDisplay(shareable_displays_, source_id);
}

SCWindow* ThumbnailCapturerMac::GetShareableWindow(SourceId source_id) const {
  return FindWindow(shareable_windows_, source_id);
}

NSArray<SCWindow*>* ThumbnailCapturerMac::SortOrderByCGWindowList(
    NSArray<SCWindow*>* current_windows) const {
  CHECK_EQ(sort_mode_, SortMode::kCGWindowList);

  // Only get on screen, non-desktop windows.
  // According to
  // https://developer.apple.com/documentation/coregraphics/cgwindowlistoption/1454105-optiononscreenonly
  // when kCGWindowListOptionOnScreenOnly is used, the order of windows are
  // in decreasing z-order.
  base::apple::ScopedCFTypeRef<CFArrayRef> window_array(
      CGWindowListCopyWindowInfo(
          kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
          kCGNullWindowID));
  if (!window_array) {
    DVLOG(2) << "Cannot sort list, nothing returned from "
                "CGWindowListCopyWindowInfo.";
    return current_windows;
  }

  // Sort `current_windows` to match the order returned
  // by CGWindowListCopyWindowInfo.
  // The windowID is the key matching entries in these containers.
  NSMutableArray<SCWindow*>* sorted_windows = [[NSMutableArray alloc] init];
  CFIndex count = CFArrayGetCount(window_array.get());
  for (CFIndex i = 0; i < count; i++) {
    CGWindowID window_id = GetWindowId(window_array.get(), i);
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

  if (capture_mode_ == CaptureMode::kSCScreenshotManager) {
    CHECK(screenshot_manager_capturer_);
    screenshot_manager_capturer_->SelectSources(ids, thumbnail_size);
    return;
  }

  // Create SCStreamConfiguration.
  SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
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
  for (SourceId id : ids) {
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
    SCContentFilter* filter = [[SCContentFilter alloc]
        initWithDesktopIndependentWindow:selected_window];

    SCKStreamDelegateAndOutput* delegate = [[SCKStreamDelegateAndOutput alloc]
        initWithSampleCallback:sample_callback
                 errorCallback:error_callback
                      sourceId:selected_window.windowID];

    // Create and start stream.
    SCStream* stream = [[SCStream alloc] initWithFilter:filter
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
    if (base::Contains(ids, it->first)) {
      ++it;
    } else {
      it = streams_.erase(it);
    }
  }
}

void ThumbnailCapturerMac::OnCapturedFrame(
    base::apple::ScopedCFTypeRef<CGImageRef> cg_image,
    ThumbnailCapturer::SourceId source_id) {
  if (!cg_image) {
    return;
  }

  // The image has been captured, pass it on to the consumer as a DesktopFrame.
  std::unique_ptr<webrtc::DesktopFrame> frame =
      webrtc::CreateDesktopFrameFromCGImage(rtc::AdoptCF(cg_image.get()));
  consumer_->OnRecurrentCaptureResult(Result::SUCCESS, std::move(frame),
                                      source_id);
}

bool ShouldUseSCContentSharingPicker() {
  if (@available(macOS 15.0, *)) {
    if (base::FeatureList::IsEnabled(media::kUseSCContentSharingPicker)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool ShouldUseThumbnailCapturerMac(DesktopMediaList::Type type) {
  // There was a bug in ScreenCaptureKit that was fixed in 14.4,
  // see b/40076027.
  if (@available(macOS 14.4, *)) {
    switch (type) {
      case DesktopMediaList::Type::kWindow:
        return ShouldUseSCContentSharingPicker() ||
               base::FeatureList::IsEnabled(
                   kScreenCaptureKitStreamPickerSonoma);
      case DesktopMediaList::Type::kScreen:
        return ShouldUseSCContentSharingPicker() ||
               base::FeatureList::IsEnabled(kScreenCaptureKitPickerScreen);
      case DesktopMediaList::Type::kNone:
      case DesktopMediaList::Type::kCurrentTab:
      case DesktopMediaList::Type::kWebContents:
        return false;
    }
  }
  if (@available(macOS 13.2, *)) {
    return base::FeatureList::IsEnabled(kScreenCaptureKitStreamPickerVentura);
  }
  return false;
}

// Creates a ThumbnailCapturerMac object. Must only be called is
// ShouldUseThumbnailCapturerMac() returns true.
std::unique_ptr<ThumbnailCapturer> CreateThumbnailCapturerMac(
    DesktopMediaList::Type type) {
  CHECK(ShouldUseThumbnailCapturerMac(type));
  if (ShouldUseSCContentSharingPicker()) {
    return std::make_unique<DesktopCapturerWrapper>(
        std::make_unique<DelegatedSourceListCapturer>(
            ConvertToDesktopMediaIDType(type)));
  }
  if (@available(macOS 13.2, *)) {
    return std::make_unique<ThumbnailCapturerMac>(type);
  }
  NOTREACHED();
}
