// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_bounds_cache.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/gfx/geometry/size.h"
#if !BUILDFLAG(IS_ANDROID)
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "media/base/media_switches.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/view.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

// The minimum window size for Document Picture-in-Picture windows. This does
// not apply to video Picture-in-Picture windows.
constexpr gfx::Size kMinWindowSize(240, 52);

// The maximum window size for Document Picture-in-Picture windows. This does
// not apply to video Picture-in-Picture windows.
constexpr double kMaxWindowSizeRatio = 0.8;

#if !BUILDFLAG(IS_ANDROID)
// Returns true if a document picture-in-picture window should be focused upon
// opening it.
bool ShouldFocusPictureInPictureWindow(const NavigateParams& params) {
  // All document picture-in-picture openings must have a source_contents.
  CHECK(params.source_contents);

  const auto* auto_picture_in_picture_tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(params.source_contents);
  if (!auto_picture_in_picture_tab_helper) {
    return true;
  }

  // The picture-in-picture window should be focused unless it's opened by the
  // AutoPictureInPictureTabHelper.
  return !auto_picture_in_picture_tab_helper->IsInAutoPictureInPicture();
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

// This web contents observer is used only for video PiP.
class PictureInPictureWindowManager::VideoWebContentsObserver final
    : public content::WebContentsObserver {
 public:
  VideoWebContentsObserver(PictureInPictureWindowManager* owner,
                           content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents), owner_(owner) {}

  ~VideoWebContentsObserver() final = default;

  void PrimaryPageChanged(content::Page& page) final {
    // Closes the active Picture-in-Picture window if user navigates away.
    owner_->CloseWindowInternal();
  }

  void WebContentsDestroyed() final { owner_->CloseWindowInternal(); }

 private:
  // Owns |this|.
  raw_ptr<PictureInPictureWindowManager> owner_ = nullptr;
};

#if !BUILDFLAG(IS_ANDROID)
// This web contents observer is used only for document PiP.
class PictureInPictureWindowManager::DocumentWebContentsObserver final
    : public content::WebContentsObserver {
 public:
  DocumentWebContentsObserver(PictureInPictureWindowManager* owner,
                              content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents), owner_(owner) {}

  ~DocumentWebContentsObserver() final = default;

  void WebContentsDestroyed() final { owner_->DocumentWebContentsDestroyed(); }

 private:
  // Owns |this|.
  raw_ptr<PictureInPictureWindowManager> owner_ = nullptr;
};
#endif  // !BUILDFLAG(IS_ANDROID)

PictureInPictureWindowManager* PictureInPictureWindowManager::GetInstance() {
  return base::Singleton<PictureInPictureWindowManager>::get();
}

void PictureInPictureWindowManager::EnterPictureInPictureWithController(
    content::PictureInPictureWindowController* pip_window_controller) {
  // If there was already a controller, close the existing window before
  // creating the next one.
  if (pip_window_controller_)
    CloseWindowInternal();

  pip_window_controller_ = pip_window_controller;

  pip_window_controller_->Show();
}

#if !BUILDFLAG(IS_ANDROID)
void PictureInPictureWindowManager::EnterDocumentPictureInPicture(
    content::WebContents* parent_web_contents,
    content::WebContents* child_web_contents) {
  // If there was already a controller, close the existing window before
  // creating the next one. This needs to happen before creating the new
  // controller so that its precondition (no child_web_contents_) remains
  // valid.
  if (pip_window_controller_)
    CloseWindowInternal();

  // Start observing the parent web contents.
  document_web_contents_observer_ =
      std::make_unique<DocumentWebContentsObserver>(this, parent_web_contents);

  auto* controller = content::PictureInPictureWindowController::
      GetOrCreateDocumentPictureInPictureController(parent_web_contents);

  controller->SetChildWebContents(child_web_contents);

  // Show the new window. As a side effect, this also first closes any
  // pre-existing PictureInPictureWindowController's window (if any).
  EnterPictureInPictureWithController(controller);

  NotifyObservers(&Observer::OnEnterPictureInPicture);
}
#endif  // !BUILDFLAG(IS_ANDROID)

content::PictureInPictureResult
PictureInPictureWindowManager::EnterVideoPictureInPicture(
    content::WebContents* web_contents) {
  // Create or update |pip_window_controller_| for the current WebContents, if
  // it is a WebContents based video PIP.
  if (!pip_window_controller_ ||
      pip_window_controller_->GetWebContents() != web_contents ||
      !pip_window_controller_->GetWebContents()->HasPictureInPictureVideo()) {
    // If there was already a video PiP controller, close the existing window
    // before creating the next one.
    if (pip_window_controller_)
      CloseWindowInternal();

    CreateWindowInternal(web_contents);
  }

  NotifyObservers(&Observer::OnEnterPictureInPicture);
  return content::PictureInPictureResult::kSuccess;
}

bool PictureInPictureWindowManager::ExitPictureInPictureViaWindowUi(
    UiBehavior behavior) {
  if (!pip_window_controller_) {
    return false;
  }

#if !BUILDFLAG(IS_ANDROID)
  // The user manually closed the pip window, so let the tab helper know in case
  // the auto-pip permission dialog was visible.
  if (auto* tab_helper = AutoPictureInPictureTabHelper::FromWebContents(
          pip_window_controller_->GetWebContents())) {
    tab_helper->OnUserClosedWindow();
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  switch (behavior) {
    case UiBehavior::kCloseWindowOnly:
      pip_window_controller_->Close(/*should_pause_video=*/false);
      break;
    case UiBehavior::kCloseWindowAndPauseVideo:
      pip_window_controller_->Close(/*should_pause_video=*/true);
      break;
    case UiBehavior::kCloseWindowAndFocusOpener:
      pip_window_controller_->CloseAndFocusInitiator();
      break;
  }

  return true;
}

bool PictureInPictureWindowManager::ExitPictureInPicture() {
  if (pip_window_controller_) {
    CloseWindowInternal();
    return true;
  }
  return false;
}

// static
void PictureInPictureWindowManager::ExitPictureInPictureSoon() {
  // Unretained is safe because we're a singleton.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
                         &PictureInPictureWindowManager::ExitPictureInPicture),
                     base::Unretained(GetInstance())));
}

void PictureInPictureWindowManager::FocusInitiator() {
  if (pip_window_controller_)
    pip_window_controller_->FocusInitiator();
}

content::WebContents* PictureInPictureWindowManager::GetWebContents() const {
  if (!pip_window_controller_)
    return nullptr;

  return pip_window_controller_->GetWebContents();
}

content::WebContents* PictureInPictureWindowManager::GetChildWebContents()
    const {
  if (!pip_window_controller_)
    return nullptr;

  return pip_window_controller_->GetChildWebContents();
}

// static
bool PictureInPictureWindowManager::IsChildWebContents(
    content::WebContents* wc) {
  auto* instance =
      base::Singleton<PictureInPictureWindowManager>::GetIfExists();
  if (!instance) {
    // No manager => no pip window.
    return false;
  }

  return instance->GetChildWebContents() == wc;
}

std::optional<gfx::Rect>
PictureInPictureWindowManager::GetPictureInPictureWindowBounds() const {
  return pip_window_controller_ ? pip_window_controller_->GetWindowBounds()
                                : std::nullopt;
}

gfx::Rect PictureInPictureWindowManager::CalculateOuterWindowBounds(
    const blink::mojom::PictureInPictureWindowOptions& pip_options,
    const display::Display& display,
    const gfx::Size& minimum_outer_window_size,
    const gfx::Size& excluded_margin) {
  // TODO(https://crbug.com/1327797): This copies a bunch of logic from
  // VideoOverlayWindowViews. That class and this one should be refactored so
  // VideoOverlayWindowViews uses PictureInPictureWindowManager to calculate
  // window sizing.
  gfx::Rect work_area = display.work_area();
  gfx::Rect window_bounds;

  // If the outer bounds for this request are cached, then ignore everything
  // else and use those.
  //
  // Typically, we have a window controller at this point, but often during
  // tests we don't.  Don't worry about the cache if it's missing.
  if (pip_window_controller_) {
    auto* const web_contents = pip_window_controller_->GetWebContents();
    std::optional<gfx::Size> requested_content_bounds;
    if (pip_options.width > 0 && pip_options.height > 0) {
      requested_content_bounds.emplace(pip_options.width, pip_options.height);
    }
    auto cached_window_bounds =
        PictureInPictureBoundsCache::GetBoundsForNewWindow(
            web_contents, display, requested_content_bounds);
    if (cached_window_bounds) {
      // Cache hit!  Just return it as the window bounds.
      return *cached_window_bounds;
    }
  }

  if (pip_options.width > 0 && pip_options.height > 0) {
    // Use width and height if we have them both, but ensure it's within the
    // required bounds.  Remember that the pip options are the desired inner
    // size, so we add any non-client size we need to convert to outer size by
    // adding back the margin around the inner area.
    gfx::Size window_size(
        base::saturated_cast<int>(pip_options.width + excluded_margin.width()),
        base::saturated_cast<int>(pip_options.height +
                                  excluded_margin.height()));
    window_size.SetToMin(GetMaximumWindowSize(display));
    window_size.SetToMax(minimum_outer_window_size);
    window_bounds = gfx::Rect(window_size);
  } else {
    // Otherwise, fall back to the aspect ratio.
    double initial_aspect_ratio = pip_options.initial_aspect_ratio > 0.0
                                      ? pip_options.initial_aspect_ratio
                                      : 1.0;
    gfx::Size window_size(work_area.width() / 5, work_area.height() / 5);
    window_size.SetToMin(GetMaximumWindowSize(display));
    window_size.SetToMax(minimum_outer_window_size);
    window_bounds = gfx::Rect(window_size);
    gfx::SizeRectToAspectRatioWithExcludedMargin(
        gfx::ResizeEdge::kTopLeft, initial_aspect_ratio,
        GetMinimumInnerWindowSize(), GetMaximumWindowSize(display),
        excluded_margin, window_bounds);
  }

  // Position the window.
  int window_diff_width = work_area.right() - window_bounds.width();
  int window_diff_height = work_area.bottom() - window_bounds.height();

  // Keep a margin distance of 2% the average of the two window size
  // differences, keeping the margins consistent.
  int buffer = (window_diff_width + window_diff_height) / 2 * 0.02;

  gfx::Point default_origin =
      gfx::Point(window_diff_width - buffer, window_diff_height - buffer);
  window_bounds.set_origin(default_origin);

  return window_bounds;
}

gfx::Rect
PictureInPictureWindowManager::CalculateInitialPictureInPictureWindowBounds(
    const blink::mojom::PictureInPictureWindowOptions& pip_options,
    const display::Display& display) {
  // Use an empty `excluded_margin`, which more or less guarantees that these
  // bounds are incorrect if `pip_options` includes a requested inner size that
  // we'd like to honor.  It's okay, because we'll recompute it later once we
  // know the excluded margin.
  return CalculateOuterWindowBounds(pip_options, display,
                                    GetMinimumInnerWindowSize(), gfx::Size());
}

void PictureInPictureWindowManager::UpdateCachedBounds(
    const gfx::Rect& most_recent_bounds) {
  // Typically, we have a window controller at this point, but often during
  // tests we don't.  Don't worry about the cache if it's missing.
  if (!pip_window_controller_) {
    return;
  }
  auto* const web_contents = pip_window_controller_->GetWebContents();
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents,
                                                  most_recent_bounds);
}

// static
gfx::Size PictureInPictureWindowManager::GetMinimumInnerWindowSize() {
  return kMinWindowSize;
}

// static
gfx::Size PictureInPictureWindowManager::GetMaximumWindowSize(
    const display::Display& display) {
  return gfx::ScaleToRoundedSize(display.size(), kMaxWindowSizeRatio);
}

// static
void PictureInPictureWindowManager::SetWindowParams(NavigateParams& params) {
#if !BUILDFLAG(IS_ANDROID)
  // Always show document picture-in-picture in a new window. When this is
  // not opened via the AutoPictureInPictureTabHelper, focus the window.
  params.window_action = ShouldFocusPictureInPictureWindow(params)
                             ? NavigateParams::SHOW_WINDOW
                             : NavigateParams::SHOW_WINDOW_INACTIVE;
#endif  // !BUILDFLAG(IS_ANDROID)
}

// static
bool PictureInPictureWindowManager::IsSupportedForDocumentPictureInPicture(
    const GURL& url) {
#if !BUILDFLAG(IS_ANDROID)
  // Only allow document PiP to be opened if the URL is of a type that we know
  // how to display in the title bar.  Otherwise, the title bar might be
  // misleading in certain scenarios.  See https://crbug.com/1460025 .
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return url.SchemeIs(url::kHttpsScheme) || url.SchemeIsFile() ||
         net::IsLocalhost(url) || url.SchemeIs(content::kChromeUIScheme);
#else
  return false;
#endif  // !BUILDFLAG(IS_ANDROID)
}

void PictureInPictureWindowManager::CreateWindowInternal(
    content::WebContents* web_contents) {
  video_web_contents_observer_ =
      std::make_unique<VideoWebContentsObserver>(this, web_contents);
  pip_window_controller_ = content::PictureInPictureWindowController::
      GetOrCreateVideoPictureInPictureController(web_contents);
}

void PictureInPictureWindowManager::CloseWindowInternal() {
  CHECK(pip_window_controller_);

  video_web_contents_observer_.reset();
  pip_window_controller_->Close(false /* should_pause_video */);
  pip_window_controller_ = nullptr;
}

#if !BUILDFLAG(IS_ANDROID)
void PictureInPictureWindowManager::DocumentWebContentsDestroyed() {
  // Document PiP window controller also observes the parent and child web
  // contents, so we only need to forget the controller here when user closes
  // the parent web contents with the PiP window open.
  document_web_contents_observer_.reset();
  if (pip_window_controller_)
    pip_window_controller_ = nullptr;
}

std::unique_ptr<AutoPipSettingOverlayView>
PictureInPictureWindowManager::GetOverlayView(
    const gfx::Rect& browser_view_overridden_bounds,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow) {
  // This should probably CHECK, but tests often can't set the controller.
  if (!pip_window_controller_) {
    return nullptr;
  }

  // This is redundant with the check for `auto_pip_tab_helper`, below.
  // However, for safety, early-out here when the flag is off.
  if (!base::FeatureList::IsEnabled(
          blink::features::kMediaSessionEnterPictureInPicture)) {
    return nullptr;
  }

  // It would be nice to create this in `EnterPictureInPicture*`, but detecting
  // auto-pip while pip is in the process of opening doesn't work.
  //
  // Remember that this can be called more than once per pip window instance,
  // such as on theme change in some cases or on Linux at any time at all, when
  // the window frame is destroyed and recreated.  Thus, one must be careful not
  // to get confused between the user closing the pip window and the pip window
  // closing itself.  Otherwise, these events would adjust the embargo counter
  // incorrectly.  As it is, we explicitly call back when the user closes the
  // pip window for that purpose.

  auto* const web_contents = pip_window_controller_->GetWebContents();
  CHECK(web_contents);

  auto* auto_pip_tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  if (!auto_pip_tab_helper) {
    return nullptr;
  }

  // See if we should display the allow / block UI.  This might call back the
  // close cb, if the pip window should be blocked.
  auto overlay_view = auto_pip_tab_helper->CreateOverlayPermissionViewIfNeeded(
      base::BindOnce(&PictureInPictureWindowManager::ExitPictureInPictureSoon),
      browser_view_overridden_bounds, anchor_view, arrow);

  if (!overlay_view) {
    // It's already allowed or blocked / embargoed.
    return nullptr;
  }

  // We need to ask the user.
  if (auto* pip_contents = GetChildWebContents()) {
    // For document pip, block input too while the permission dialog is shown.
    overlay_view->IgnoreInputEvents(pip_contents);
  }

  return overlay_view;
}

PictureInPictureOcclusionTracker*
PictureInPictureWindowManager::GetOcclusionTracker() {
  CreateOcclusionTrackerIfNecessary();
  return occlusion_tracker_.get();
}

void PictureInPictureWindowManager::CreateOcclusionTrackerIfNecessary() {
  if (occlusion_tracker_) {
    return;
  }

  if (base::FeatureList::IsEnabled(media::kPictureInPictureOcclusionTracking)) {
    occlusion_tracker_ = std::make_unique<PictureInPictureOcclusionTracker>();
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

PictureInPictureWindowManager::PictureInPictureWindowManager() = default;

PictureInPictureWindowManager::~PictureInPictureWindowManager() = default;
