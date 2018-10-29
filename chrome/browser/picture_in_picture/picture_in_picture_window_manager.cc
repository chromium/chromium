// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/size.h"

class PictureInPictureWindowManager::ContentsObserver
    : public content::WebContentsObserver {
 public:
  ContentsObserver(PictureInPictureWindowManager* owner,
                   content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents), owner_(owner) {}

  ~ContentsObserver() final = default;

  void DidFinishNavigation(content::NavigationHandle* navigation_handle) final {
    // Closes the active Picture-in-Picture window if user navigates away.
    if (!navigation_handle->IsInMainFrame() ||
        !navigation_handle->HasCommitted() ||
        navigation_handle->IsSameDocument()) {
      return;
    }
    owner_->CloseWindowInternal(true /* should_reset_pip_player */);
  }

  void WebContentsDestroyed() final {
    owner_->CloseWindowInternal(true /* should_reset_pip_player */);
  }

 private:
  // Owns |this|.
  PictureInPictureWindowManager* owner_ = nullptr;
};

PictureInPictureWindowManager* PictureInPictureWindowManager::GetInstance() {
  return base::Singleton<PictureInPictureWindowManager>::get();
}

void PictureInPictureWindowManager::EnterPictureInPictureWithController(
    content::PictureInPictureWindowController* pip_window_controller) {
  // If there was already a controller, close the existing window before
  // creating the next one.
  if (pip_window_controller_)
    CloseWindowInternal(false /* should_reset_pip_player */);

  pip_window_controller_ = pip_window_controller;

  pip_window_controller_->Show();
}

gfx::Size PictureInPictureWindowManager::EnterPictureInPicture(
    content::WebContents* web_contents,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  // If there was already a controller, close the existing window before
  // creating the next one.
  if (pip_window_controller_)
    CloseWindowInternal(false /* should_reset_pip_player */);

  // Create or update |pip_window_controller_| for the current WebContents, if
  // it is a WebContents based PIP.
  if (!pip_window_controller_ ||
      (pip_window_controller_->GetInitiatorWebContents() != nullptr &&
       pip_window_controller_->GetInitiatorWebContents() != web_contents)) {
    CreateWindowInternal(web_contents);
  }

  pip_window_controller_->EmbedSurface(surface_id, natural_size);
  return pip_window_controller_->Show();
}

void PictureInPictureWindowManager::ExitPictureInPicture() {
  if (pip_window_controller_)
    CloseWindowInternal(true /* should_reset_pip_player */);
}

content::WebContents* PictureInPictureWindowManager::GetWebContents() {
  if (!pip_window_controller_)
    return nullptr;

  return pip_window_controller_->GetInitiatorWebContents();
}

void PictureInPictureWindowManager::CreateWindowInternal(
    content::WebContents* web_contents) {
  contents_observer_ = std::make_unique<ContentsObserver>(this, web_contents);
  pip_window_controller_ =
      content::PictureInPictureWindowController::GetOrCreateForWebContents(
          web_contents);
}

void PictureInPictureWindowManager::CloseWindowInternal(
    bool should_reset_pip_player) {
  DCHECK(pip_window_controller_);

  contents_observer_.reset();
  pip_window_controller_->Close(false /* should_pause_video */,
                                should_reset_pip_player);
  pip_window_controller_ = nullptr;
}

PictureInPictureWindowManager::PictureInPictureWindowManager() = default;

PictureInPictureWindowManager::~PictureInPictureWindowManager() = default;
