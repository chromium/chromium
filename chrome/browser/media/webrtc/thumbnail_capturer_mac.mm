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

// Default max frame rate that is used when capturing thumbnails. This is per
// source so the combined frame rate can be higher if there are multiple
// sources.
constexpr int kDefaultMaxFrameRate = 1;
// Refresh interval that is used to query for the list of shareable content.
constexpr base::TimeDelta kSourceListRefreshTimerInterval =
    base::Milliseconds(250);

bool API_AVAILABLE(macos(13.2)) IncludeWindowInSourceList(SCWindow* window) {
  // On macOS 14, each window that is captured has an indicator that the window
  // is being captured. This indicator is a window itself. Filter out these
  // windows based on their very small height.
  // The condition on window layer is used to filter out other components that
  // are treated as windows but should not be captured.
  constexpr int kMinimumWindowHeight = 26;
  return window.windowLayer == 0 &&
         window.frame.size.height >= kMinimumWindowHeight;
}

class API_AVAILABLE(macos(13.2)) ThumbnailCapturerMac
    : public ThumbnailCapturer {
 public:
  ThumbnailCapturerMac(const gfx::Size& thumbnail_size);
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

  void SelectSources(const std::vector<SourceId>& ids) override;

 private:
  struct StreamAndDelegate {
    SCStream* __strong stream;
    SCKStreamDelegateAndOutput* __strong delegate;
  };

  void UpdateWindowsList();
  void OnShareableContentCreated(SCShareableContent* content);
  void OnCapturedFrame(CGImageRef image, SourceId source_id);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const gfx::Size thumbnail_size_;
  int max_frame_rate_ = kDefaultMaxFrameRate;
  raw_ptr<Consumer> consumer_;
  NSArray<SCWindow*>* __strong shareable_windows_;
  std::unordered_map<SourceId, StreamAndDelegate> streams_;
  base::RepeatingTimer refresh_timer_;
  base::WeakPtrFactory<ThumbnailCapturerMac> weak_factory_{this};
};

ThumbnailCapturerMac::ThumbnailCapturerMac(const gfx::Size& thumbnail_size)
    : thumbnail_size_(thumbnail_size) {}

void ThumbnailCapturerMac::Start(Consumer* consumer) {
  consumer_ = consumer;
  task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  UpdateWindowsList();
  // Start a timer that periodically update the list of sharable windows.
  refresh_timer_.Start(FROM_HERE, kSourceListRefreshTimerInterval, this,
                       &ThumbnailCapturerMac::UpdateWindowsList);
}

void ThumbnailCapturerMac::SetMaxFrameRate(uint32_t max_frame_rate) {
  max_frame_rate_ = max_frame_rate;
}

bool ThumbnailCapturerMac::GetSourceList(SourceList* sources) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  sources->clear();
  if (!shareable_windows_) {
    return false;
  }

  // TODO(https://crbug.com/1478176): Filter out windows with empty titles.
  // Let W be the set of all windows for which IncludeWindowInSourceList(w)
  // returns true for all w in W. Let X be the subset of W so that all windows x
  // in X belong to application Y. Only allow f(X) windows with empty titles,
  // where f(X) = 0 if there exists one window in X whose title is not empty.
  // f(X) = 1 if all windows in X have empty titles.
  for (SCWindow* window in shareable_windows_) {
    if (IncludeWindowInSourceList(window)) {
      sources->push_back(ThumbnailCapturer::Source{
          window.windowID, base::SysNSStringToUTF8(window.title)});
    }
  }
  return true;
}

void ThumbnailCapturerMac::UpdateWindowsList() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto content_callback = base::BindPostTask(
      task_runner_,
      base::BindRepeating(&ThumbnailCapturerMac::OnShareableContentCreated,
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

void ThumbnailCapturerMac::OnShareableContentCreated(
    SCShareableContent* content) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!content) {
    return;
  }

  shareable_windows_ = [content windows];

  // Filter out windows that should not be shareable.
  std::unordered_set<SourceId> source_list_windows;
  for (SCWindow* window in shareable_windows_) {
    if (IncludeWindowInSourceList(window)) {
      source_list_windows.insert(window.windowID);
    }
  }

  // Remove all streams for windows that are not active anymore. New streams are
  // created once the consumer calls SelectSources().
  for (auto it = streams_.begin(); it != streams_.end();) {
    if (source_list_windows.find(it->first) == source_list_windows.end()) {
      it = streams_.erase(it);
    } else {
      ++it;
    }
  }

  // TODO(https://crbug.com/1471931): Only call update if the list is changed:
  // windows opened/closed, order of the list, and title.
  consumer_->OnSourceListUpdated();
}

void ThumbnailCapturerMac::SelectSources(const std::vector<SourceId>& ids) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Create SCStreamConfiguration.
  SCStreamConfiguration* __strong config = [[SCStreamConfiguration alloc] init];
  config.width = thumbnail_size_.width();
  config.height = thumbnail_size_.height();
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

// Creates a ThumbnailCaptureMac object. Must only be called is
// ShouldUseThumbnailCapturerMac() returns true.
std::unique_ptr<ThumbnailCapturer> CreateThumbnailCapturerMac(
    const gfx::Size& thumbnail_size) {
  CHECK(ShouldUseThumbnailCapturerMac());
  if (@available(macOS 13.2, *)) {
    return std::make_unique<ThumbnailCapturerMac>(thumbnail_size);
  }
  NOTREACHED_NORETURN();
}
