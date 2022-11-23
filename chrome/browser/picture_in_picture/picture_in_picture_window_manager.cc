// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"

#include "base/memory/raw_ptr.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/size.h"

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

absl::optional<gfx::Rect>
PictureInPictureWindowManager::GetPictureInPictureWindowBounds() const {
  return pip_window_controller_ ? pip_window_controller_->GetWindowBounds()
                                : absl::nullopt;
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
