// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
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

namespace {

// The minimum window size for Document Picture-in-Picture windows. This does
// not apply to video Picture-in-Picture windows.
constexpr gfx::Size kMinWindowSize(300, 300);

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

  return content::PictureInPictureResult::kSuccess;
}

void PictureInPictureWindowManager::ExitPictureInPicture() {
  if (pip_window_controller_)
    CloseWindowInternal();
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

// static
gfx::Rect
PictureInPictureWindowManager::CalculateInitialPictureInPictureWindowBounds(
    const blink::mojom::PictureInPictureWindowOptions& pip_options,
    const display::Display& display) {
  // TODO(https://crbug.com/1327797): This copies a bunch of logic from
  // VideoOverlayWindowViews. That class and this one should be refactored so
  // VideoOverlayWindowViews uses PictureInPictureWindowManager to calculate
  // window sizing.
  gfx::Rect work_area = display.work_area();
  gfx::Rect window_bounds;
  if (pip_options.width > 0 && pip_options.height > 0) {
    // Use width and height if we have them both, but ensure it's within the
    // required bounds.
    gfx::Size window_size(base::saturated_cast<int>(pip_options.width),
                          base::saturated_cast<int>(pip_options.height));
    window_size.SetToMin(GetMaximumWindowSize(display));
    window_size.SetToMax(GetMinimumWindowSize());
    window_bounds = gfx::Rect(window_size);
  } else {
    // Otherwise, fall back to the aspect ratio.
    double initial_aspect_ratio = pip_options.initial_aspect_ratio > 0.0
                                      ? pip_options.initial_aspect_ratio
                                      : 1.0;
    gfx::Size window_size(work_area.width() / 5, work_area.height() / 5);
    window_size.SetToMin(GetMaximumWindowSize(display));
    window_size.SetToMax(GetMinimumWindowSize());
    window_bounds = gfx::Rect(window_size);
    gfx::SizeRectToAspectRatio(gfx::ResizeEdge::kTopLeft, initial_aspect_ratio,
                               GetMinimumWindowSize(),
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

// static
gfx::Size PictureInPictureWindowManager::GetMinimumWindowSize() {
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
  DCHECK(pip_window_controller_);

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
#endif  // !BUILDFLAG(IS_ANDROID)

PictureInPictureWindowManager::PictureInPictureWindowManager() = default;

PictureInPictureWindowManager::~PictureInPictureWindowManager() = default;
