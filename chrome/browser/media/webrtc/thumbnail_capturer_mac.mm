// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/thumbnail_capturer_mac.h"

#include <CoreGraphics/CoreGraphics.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <VideoToolbox/VideoToolbox.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <optional>
#include <unordered_map>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/barrier_closure.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/media/webrtc/delegated_source_list_capturer.h"
#include "chrome/browser/media/webrtc/desktop_capturer_wrapper.h"
#include "chrome/browser/media/webrtc/desktop_media_list_base.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "media/capture/video_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/mac/desktop_frame_utils.h"

using SampleCallback =
    base::RepeatingCallback<void(base::apple::ScopedCFTypeRef<CGImageRef> image,
                                 ThumbnailCapturer::SourceId source_id)>;

namespace {

// These features enable the use of ScreenCaptureKit to produce thumbnails in
// the getDisplayMedia picker. The `kScreenCaptureKitStreamPicker*` features are
// used to produce thumbnails of specific windows, while the
// `kScreenCaptureKitPickerScreen` feature is used to produce thumbnails of the
// entire screen. This is distinct from `kUseSCContentSharingPicker`, which uses
// the native macOS picker.
BASE_FEATURE(kScreenCaptureKitStreamPickerSonoma,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kScreenCaptureKitPickerScreen, base::FEATURE_ENABLED_BY_DEFAULT);

// Default max frame rate that is used when capturing thumbnails. This is per
// source so the combined frame rate can be higher if there are multiple
// sources.
constexpr int kThumbnailCapturerMacMaxFrameRate = 1;

// Refresh interval that is used to query for the list of shareable content.
constexpr base::TimeDelta kThumbnailCapturerMacRefreshTimerInterval =
    base::Milliseconds(250);

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
  NOTREACHED();
}

// The minimum window size that is still considered to be a shareable window.
// Windows with smaller height or width are filtered out.
constexpr int kThumbnailCapturerMacMinWindowSize = 40;

// The maximum number of sources that can be captured in each capture cycle
// if the capture mode is set to kSCScreenshotManager.  This is also the
// maximum number of capture calls that can be in-flight simultaneously.
// By having a limit and cycling through what windows are captured we get a
// graceful degradation.
constexpr size_t kThumbnailCapturerMacMaxSourcesPerCycle = 25;

std::map<content::DesktopMediaID::Id,
         std::optional<content::DesktopMediaID::Id>>
GetPipIdToExcludeFromScreenCaptureOnUIThread(
    std::vector<content::DesktopMediaID::Id> screen_ids,
    content::GlobalRenderFrameHostId render_frame_host_id,
    PipWebContentsGetter pip_web_contents_getter,
    PipWindowToExcludeForScreenCaptureGetter
        pip_window_to_exclude_for_screen_capture_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = pip_web_contents_getter.Run();
  if (!web_contents) {
    return {};
  }

  // The PiP window is eligible for exclusion only if the current tab owns the
  // PiP window.
  content::GlobalRenderFrameHostId pip_owner_render_frame_host_id =
      web_contents->GetPrimaryMainFrame()->GetGlobalId();
  if (pip_owner_render_frame_host_id != render_frame_host_id) {
    return {};
  }

  std::map<content::DesktopMediaID::Id,
           std::optional<content::DesktopMediaID::Id>>
      screen_exclude_pip_ids;
  for (const auto& screen_id : screen_ids) {
    std::optional<content::DesktopMediaID::Id> excluded_id =
        pip_window_to_exclude_for_screen_capture_getter.Run(screen_id);
    screen_exclude_pip_ids[screen_id] = excluded_id;
  }
  return screen_exclude_pip_ids;
}

// Given SCShareableContent and an optional `DesktopMediaID::Id`, returns an
// array containing the SCWindow object corresponding to the provided ID.
// Returns an empty array if the ID is not provided or no matching window is
// found.
API_AVAILABLE(macos(12.3))
NSArray<SCWindow*>* ConvertWindowIDToSCWindows(
    SCShareableContent* content,
    std::optional<content::DesktopMediaID::Id> excluded_window_id) {
  if (!excluded_window_id) {
    return @[];
  }

  for (SCWindow* window_to_check in content.windows) {
    if (window_to_check.windowID ==
        static_cast<CGWindowID>(*excluded_window_id)) {
      return @[ window_to_check ];
    }
  }
  return @[];
}

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
  using ContentFilterCallback = base::RepeatingCallback<
      SCContentFilter*(ThumbnailCapturer::SourceId source_id, CGRect& frame)>;

  ScreenshotManagerCapturer(
      DesktopMediaList::Type type,
      int max_frame_rate,
      ContentFilterCallback get_display_content_filter_callback,
      ContentFilterCallback get_window_content_filter_callback,
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

  // Callback to retrieve an SCContentFilter* for a display based on source ID.
  ContentFilterCallback get_display_content_filter_callback_;

  // Callback to retrieve an SCContentFilter* for a window based on source ID.
  ContentFilterCallback get_window_content_filter_callback_;

  // Callback that is used whenever a thumbnail is captured.
  SampleCallback sample_callback_;

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
    ContentFilterCallback get_display_content_filter_callback,
    ContentFilterCallback get_window_content_filter_callback,
    SampleCallback sample_callback)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      type_(type),
      get_display_content_filter_callback_(get_display_content_filter_callback),
      get_window_content_filter_callback_(get_window_content_filter_callback),
      sample_callback_(sample_callback) {
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
    if (!std::ranges::contains(selected_sources_, source_id)) {
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
  CHECK_LE(capture_calls_in_flight_, kThumbnailCapturerMacMaxSourcesPerCycle);
  size_t sources_to_capture =
      std::min(capture_queue_.size(), kThumbnailCapturerMacMaxSourcesPerCycle -
                                          capture_calls_in_flight_);
  for (size_t i = 0; i < sources_to_capture; ++i) {
    ThumbnailCapturer::SourceId source_id = capture_queue_.front();
    capture_queue_.pop_front();
    if (!std::ranges::contains(selected_sources_, source_id)) {
      continue;
    }
    CaptureSource(source_id);
  }
}

void ScreenshotManagerCapturer::CaptureSource(
    ThumbnailCapturer::SourceId source_id) {
  CGRect frame = CGRectZero;
  SCContentFilter* filter;
  switch (type_) {
    case DesktopMediaList::Type::kScreen: {
      filter = get_display_content_filter_callback_.Run(source_id, frame);
      break;
    }
    case DesktopMediaList::Type::kWindow: {
      filter = get_window_content_filter_callback_.Run(source_id, frame);
      break;
    }
    case DesktopMediaList::Type::kNone:
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      NOTREACHED();
  }

  if (!filter) {
    return;
  }
  SCScreenshotCaptureSource(filter, frame, source_id);
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

// Context object to hold the results of the asynchronous operations (PiP IDs
// and shareable content) so they can be processed together when both complete.
struct API_AVAILABLE(macos(12.3)) UpdateContext
    : public base::RefCountedThreadSafe<UpdateContext> {
  std::map<content::DesktopMediaID::Id,
           std::optional<content::DesktopMediaID::Id>>
      pip_ids;
  SCShareableContent* __strong content;

 private:
  friend class base::RefCountedThreadSafe<UpdateContext>;
  ~UpdateContext() = default;
};

class API_AVAILABLE(macos(13.2)) ThumbnailCapturerMac
    : public ThumbnailCapturer {
 public:
  ThumbnailCapturerMac(DesktopMediaList::Type type,
                       content::GlobalRenderFrameHostId render_frame_host_id,
                       PipWebContentsGetter pip_web_contents_getter,
                       PipWindowToExcludeForScreenCaptureGetter
                           pip_window_to_exclude_for_screen_capture_getter);
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
  void SetSortWindowListForTesting(bool sort) { sort_window_list_ = sort; }

 private:
  void UpdateWindowsList();
  void OnUpdateComplete(scoped_refptr<UpdateContext> context);

  void GetPipIds(scoped_refptr<UpdateContext> context,
                 base::RepeatingClosure barrier);
  void GetShareableContent(scoped_refptr<UpdateContext> context,
                           base::RepeatingClosure barrier);

  void GetDisplaySourceList(SourceList* sources) const;
  void GetWindowSourceList(SourceList* sources) const;

  void UpdateShareableWindows(NSArray<SCWindow*>* content_windows);
  SCContentFilter* GetDisplayContentFilter(SourceId source_id,
                                           CGRect& frame) const;
  SCContentFilter* GetWindowContentFilter(SourceId source_id,
                                          CGRect& frame) const;

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
  void OnCapturedFrame(base::apple::ScopedCFTypeRef<CGImageRef> image,
                       ThumbnailCapturer::SourceId source_id);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DesktopMediaList::Type type_;
  const content::GlobalRenderFrameHostId render_frame_host_id_;
  int max_frame_rate_;
  raw_ptr<Consumer> consumer_;
  int shareable_content_callbacks_ = 0;
  int shareable_content_errors_ = 0;

  // A cache of the shareable windows and shareable displays. sharable_windows_
  // is used to produce the source list. shareable_displays_ is used to
  // determine if a window is fullscreen or not. Both are updated continuously
  // by the refresh_timer_.
  NSArray<SCWindow*>* __strong shareable_windows_;
  NSArray<SCDisplay*>* __strong shareable_displays_;

  // A cache of the PiP window IDs that should be excluded from the capture of
  // the specified `desktop_id`.
  NSDictionary<NSNumber*, NSArray<SCWindow*>*>* __strong
      excluded_windows_by_screen_ = @{};

  base::RepeatingTimer refresh_timer_;

  // Used for testing to disable the sorting of windows using CGWindowList.
  bool sort_window_list_ = true;
  bool update_in_progress_ = false;

  std::unique_ptr<ScreenshotManagerCapturer> screenshot_manager_capturer_;

  // Used for dependency injection.
  PipWebContentsGetter pip_web_contents_getter_;
  PipWindowToExcludeForScreenCaptureGetter
      pip_window_to_exclude_for_screen_capture_getter_;

  base::WeakPtrFactory<ThumbnailCapturerMac> weak_factory_{this};
};

ThumbnailCapturerMac::ThumbnailCapturerMac(
    DesktopMediaList::Type type,
    content::GlobalRenderFrameHostId render_frame_host_id,
    PipWebContentsGetter pip_web_contents_getter,
    PipWindowToExcludeForScreenCaptureGetter
        pip_window_to_exclude_for_screen_capture_getter)
    : type_(type),
      render_frame_host_id_(render_frame_host_id),
      max_frame_rate_(kThumbnailCapturerMacMaxFrameRate),
      shareable_windows_([[NSArray<SCWindow*> alloc] init]),
      pip_web_contents_getter_(std::move(pip_web_contents_getter)),
      pip_window_to_exclude_for_screen_capture_getter_(
          std::move(pip_window_to_exclude_for_screen_capture_getter)) {
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
  refresh_timer_.Start(FROM_HERE, kThumbnailCapturerMacRefreshTimerInterval,
                       this, &ThumbnailCapturerMac::UpdateWindowsList);

  CHECK(!screenshot_manager_capturer_);
  // Unretained is safe because `screenshot_manager_capturer_ ` is owned by
  // `this`, and hence has a shorter lifetime than `this`.
  screenshot_manager_capturer_ = std::make_unique<ScreenshotManagerCapturer>(
      type_, max_frame_rate_,
      base::BindRepeating(&ThumbnailCapturerMac::GetDisplayContentFilter,
                          base::Unretained(this)),
      base::BindRepeating(&ThumbnailCapturerMac::GetWindowContentFilter,
                          base::Unretained(this)),
      base::BindRepeating(&ThumbnailCapturerMac::OnCapturedFrame,
                          weak_factory_.GetWeakPtr()));
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
    if (!application_to_window_count.contains(pid)) {
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

  if (update_in_progress_) {
    return;
  }
  update_in_progress_ = true;

  const bool is_screen_capture = type_ == DesktopMediaList::Type::kScreen;
  auto context = base::MakeRefCounted<UpdateContext>();
  // The barrier closure will be called once for PiP IDs (if kScreen) and once
  // for shareable content.
  auto barrier = base::BarrierClosure(
      is_screen_capture ? 2 : 1,
      base::BindPostTask(task_runner_,
                         base::BindOnce(&ThumbnailCapturerMac::OnUpdateComplete,
                                        weak_factory_.GetWeakPtr(), context)));

  if (is_screen_capture) {
    GetPipIds(context, barrier);
  }

  GetShareableContent(context, barrier);
}

void ThumbnailCapturerMac::GetPipIds(scoped_refptr<UpdateContext> context,
                                     base::RepeatingClosure barrier) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  std::vector<content::DesktopMediaID::Id> screen_ids;
  for (SCDisplay* display in shareable_displays_) {
    screen_ids.push_back(display.displayID);
  }

  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetPipIdToExcludeFromScreenCaptureOnUIThread, screen_ids,
                     render_frame_host_id_, pip_web_contents_getter_,
                     pip_window_to_exclude_for_screen_capture_getter_),
      base::BindOnce(
          [](scoped_refptr<UpdateContext> context,
             base::RepeatingClosure barrier,
             std::map<content::DesktopMediaID::Id,
                      std::optional<content::DesktopMediaID::Id>> pip_ids) {
            context->pip_ids = std::move(pip_ids);
            barrier.Run();
          },
          context, barrier));
}

void ThumbnailCapturerMac::GetShareableContent(
    scoped_refptr<UpdateContext> context,
    base::RepeatingClosure barrier) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto handler = base::CallbackToBlock(base::BindOnce(
      [](scoped_refptr<UpdateContext> context, base::RepeatingClosure barrier,
         SCShareableContent* content, NSError* error) {
        if (content && !error) {
          context->content = content;
        }
        barrier.Run();
      },
      context, barrier));

  // Exclude desktop windows (e.g., background image and deskktop icons) and
  // windows that are not on screen (e.g., minimized and behind fullscreen
  // windows).
  [SCShareableContent getShareableContentExcludingDesktopWindows:true
                                             onScreenWindowsOnly:true
                                               completionHandler:handler];
}

void ThumbnailCapturerMac::OnUpdateComplete(
    scoped_refptr<UpdateContext> context) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  update_in_progress_ = false;
  std::map<content::DesktopMediaID::Id,
           std::optional<content::DesktopMediaID::Id>>
      excluded_pip_ids_by_screen = std::move(context->pip_ids);
  SCShareableContent* content = context->content;

  ++shareable_content_callbacks_;
  if (!content) {
    DVLOG(2) << "Could not get shareable content.";
    ++shareable_content_errors_;
    return;
  }

  shareable_displays_ = [content displays];
  UpdateShareableWindows([content windows]);

  auto* excluded_windows_by_screen =
      [[NSMutableDictionary<NSNumber*, NSArray<SCWindow*>*> alloc] init];
  for (auto const& [screen_id, pip_id] : excluded_pip_ids_by_screen) {
    [excluded_windows_by_screen
        setObject:ConvertWindowIDToSCWindows(content, pip_id)
           forKey:@(screen_id)];
  }
  excluded_windows_by_screen_ = excluded_windows_by_screen;

  // TODO(crbug.com/40278456): Only call update if the list is changed:
  // windows opened/closed, order of the list, and title.
  consumer_->OnSourceListUpdated();
}

void ThumbnailCapturerMac::UpdateShareableWindows(
    NSArray<SCWindow*>* content_windows) {
  // Narrow down the list to shareable windows.
  content_windows = FilterOutUnshareable(content_windows);

  // Update shareable_streams_ from current_windows.
  shareable_windows_ = SortOrderByCGWindowList(content_windows);
}

SCContentFilter* ThumbnailCapturerMac::GetDisplayContentFilter(
    SourceId source_id,
    CGRect& frame) const {
  SCDisplay* display = FindDisplay(shareable_displays_, source_id);
  if (!display) {
    return nil;
  }
  frame = [display frame];
  NSArray<SCWindow*>* excluded_windows =
      [excluded_windows_by_screen_ objectForKey:@(source_id)];
  return [[SCContentFilter alloc]
       initWithDisplay:display
      excludingWindows:excluded_windows ? excluded_windows : @[]];
}

SCContentFilter* ThumbnailCapturerMac::GetWindowContentFilter(
    SourceId source_id,
    CGRect& frame) const {
  SCWindow* window = FindWindow(shareable_windows_, source_id);
  if (!window) {
    return nil;
  }
  frame = [window frame];
  return [[SCContentFilter alloc] initWithDesktopIndependentWindow:window];
}

NSArray<SCWindow*>* ThumbnailCapturerMac::SortOrderByCGWindowList(
    NSArray<SCWindow*>* current_windows) const {
  if (!sort_window_list_) {
    return current_windows;
  }

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
         window.frame.size.height >= kThumbnailCapturerMacMinWindowSize &&
         window.frame.size.width >= kThumbnailCapturerMacMinWindowSize;
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

void ThumbnailCapturerMac::SelectSources(const std::vector<SourceId>& ids,
                                         gfx::Size thumbnail_size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(screenshot_manager_capturer_);
  screenshot_manager_capturer_->SelectSources(ids, thumbnail_size);
}

void ThumbnailCapturerMac::OnCapturedFrame(
    base::apple::ScopedCFTypeRef<CGImageRef> cg_image,
    ThumbnailCapturer::SourceId source_id) {
  if (!cg_image) {
    return;
  }

  // The image has been captured, pass it on to the consumer as a DesktopFrame.
  std::unique_ptr<webrtc::DesktopFrame> frame =
      webrtc::CreateDesktopFrameFromCGImage(webrtc::AdoptCF(cg_image.get()));
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
  return false;
}

// Creates a ThumbnailCapturerMac object. Must only be called if
// ShouldUseThumbnailCapturerMac() returns true.
std::unique_ptr<ThumbnailCapturer> CreateThumbnailCapturerMac(
    DesktopMediaList::Type type,
    content::WebContents* web_contents) {
  CHECK(ShouldUseThumbnailCapturerMac(type));
  if (ShouldUseSCContentSharingPicker()) {
    return std::make_unique<DesktopCapturerWrapper>(
        std::make_unique<DelegatedSourceListCapturer>(
            ConvertToDesktopMediaIDType(type)));
  }

  content::GlobalRenderFrameHostId render_frame_host_id;
  if (web_contents) {
    render_frame_host_id = web_contents->GetPrimaryMainFrame()->GetGlobalId();
  }

  if (@available(macOS 14.4, *)) {
    auto pip_web_contents_getter = base::BindRepeating([]() {
      return PictureInPictureWindowManager::GetInstance()->GetWebContents();
    });
    auto pip_window_to_exclude_getter =
        base::BindRepeating([](content::DesktopMediaID::Id screen_id) {
          return content::desktop_capture::
              GetPipWindowToExcludeFromScreenCapture(screen_id);
        });

    return std::make_unique<ThumbnailCapturerMac>(
        type, render_frame_host_id, std::move(pip_web_contents_getter),
        std::move(pip_window_to_exclude_getter));
  }
  NOTREACHED();
}

std::unique_ptr<ThumbnailCapturer> CreateThumbnailCapturerMacForTesting(
    DesktopMediaList::Type type,
    content::GlobalRenderFrameHostId render_frame_host_id,
    PipWebContentsGetter pip_web_contents_getter,
    PipWindowToExcludeForScreenCaptureGetter
        pip_window_to_exclude_for_screen_capture_getter) {
  if (@available(macOS 14.4, *)) {
    if (!pip_web_contents_getter) {
      pip_web_contents_getter = base::BindRepeating(
          []() -> content::WebContents* { return nullptr; });
    }
    if (!pip_window_to_exclude_for_screen_capture_getter) {
      pip_window_to_exclude_for_screen_capture_getter = base::BindRepeating(
          [](content::DesktopMediaID::Id)
              -> std::optional<content::DesktopMediaID::Id> {
            return std::nullopt;
          });
    }
    auto capturer = std::make_unique<ThumbnailCapturerMac>(
        type, render_frame_host_id, std::move(pip_web_contents_getter),
        std::move(pip_window_to_exclude_for_screen_capture_getter));
    capturer->SetSortWindowListForTesting(false);  // IN-TEST
    return capturer;
  }
  NOTREACHED();
}
