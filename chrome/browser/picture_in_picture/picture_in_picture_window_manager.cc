// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_bounds_cache.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/gfx/geometry/size.h"
#if !BUILDFLAG(IS_ANDROID)
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/view.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

// The minimum window size for Document Picture-in-Picture windows. This does
// not apply to video Picture-in-Picture windows.
constexpr gfx::Size kMinWindowSize(240, 52);

// The maximum window size for Document Picture-in-Picture windows. This does
// not apply to video Picture-in-Picture windows.
constexpr double kMaxWindowSizeRatio = 0.8;

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

absl::optional<gfx::Rect>
PictureInPictureWindowManager::GetPictureInPictureWindowBounds() const {
  return pip_window_controller_ ? pip_window_controller_->GetWindowBounds()
                                : absl::nullopt;
}

gfx::Rect PictureInPictureWindowManager::CalculatePictureInPictureWindowBounds(
    const blink::mojom::PictureInPictureWindowOptions& pip_options,
    const display::Display& display,
    const gfx::Size& minimum_outer_window_size) {
  // TODO(https://crbug.com/1327797): This copies a bunch of logic from
  // VideoOverlayWindowViews. That class and this one should be refactored so
  // VideoOverlayWindowViews uses PictureInPictureWindowManager to calculate
  // window sizing.
  gfx::Rect work_area = display.work_area();
  gfx::Rect window_bounds;

  // Typically, we have a window controller at this point, but often during
  // tests we don't.  Don't worry about the cache if it's missing.
  if (pip_window_controller_) {
    auto* const web_contents = pip_window_controller_->GetWebContents();
    absl::optional<gfx::Size> requested_content_bounds;
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
    // required bounds.
    gfx::Size window_size(base::saturated_cast<int>(pip_options.width),
                          base::saturated_cast<int>(pip_options.height));
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
    gfx::SizeRectToAspectRatio(gfx::ResizeEdge::kTopLeft, initial_aspect_ratio,
                               GetMinimumInnerWindowSize(),
                               GetMaximumWindowSize(display), &window_bounds);
  }

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
  return CalculatePictureInPictureWindowBounds(pip_options, display,
                                               GetMinimumInnerWindowSize());
}

gfx::Rect PictureInPictureWindowManager::AdjustPictureInPictureWindowBounds(
    const blink::mojom::PictureInPictureWindowOptions& pip_options,
    const display::Display& display,
    const gfx::Size& minimum_window_size) {
  return CalculatePictureInPictureWindowBounds(pip_options, display,
                                               minimum_window_size);
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
#if !BUILDFLAG(IS_ANDROID)
  auto_pip_setting_helper_.reset();
#endif  // !BUILDFLAG(IS_ANDROID)
}

#if !BUILDFLAG(IS_ANDROID)
void PictureInPictureWindowManager::DocumentWebContentsDestroyed() {
  // Document PiP window controller also observes the parent and child web
  // contents, so we only need to forget the controller here when user closes
  // the parent web contents with the PiP window open.
  document_web_contents_observer_.reset();
  // `setting_helper_` depends on the opener's WebContents.
  auto_pip_setting_helper_.reset();
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

  auto* const web_contents = pip_window_controller_->GetWebContents();

  auto* auto_pip_tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  if (!auto_pip_tab_helper ||
      !auto_pip_tab_helper->IsInAutoPictureInPicture()) {
    // This isn't auto-pip, so the content setting doesn't matter.
    return nullptr;
  }

  auto auto_pip_setting_helper = AutoPipSettingHelper::CreateForWebContents(
      web_contents,
      base::BindOnce(&PictureInPictureWindowManager::ExitPictureInPictureSoon));

  auto overlay_view = auto_pip_setting_helper->CreateOverlayViewIfNeeded(
      browser_view_overridden_bounds, anchor_view, arrow);
  if (overlay_view) {
    // Retain the setting helper for the overlay view, and add the overlay view.
    auto_pip_setting_helper_ = std::move(auto_pip_setting_helper);
  }

  return overlay_view;
}
#endif  // !BUILDFLAG(IS_ANDROID)

std::vector<url::Origin>
PictureInPictureWindowManager::GetActiveSessionOrigins() {
  std::vector<url::Origin> active_origins;
  if (pip_window_controller_ &&
      pip_window_controller_->GetOrigin().has_value()) {
    active_origins.push_back(pip_window_controller_->GetOrigin().value());
  }
  return active_origins;
}

PictureInPictureWindowManager::PictureInPictureWindowManager() = default;

PictureInPictureWindowManager::~PictureInPictureWindowManager() = default;
