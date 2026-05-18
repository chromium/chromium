// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_window_occlusion_helper_web_contents.h"

#include "content/public/browser/web_contents.h"

// static
std::unique_ptr<AutoPictureInPictureWindowOcclusionHelperBase>
AutoPictureInPictureWindowOcclusionHelperBase::CreatePlatformHelper(
    content::WebContents* web_contents,
    OcclusionStateChangedCallback callback) {
  return std::make_unique<AutoPictureInPictureWindowOcclusionHelperWebContents>(
      web_contents, std::move(callback));
}

AutoPictureInPictureWindowOcclusionHelperWebContents::
    AutoPictureInPictureWindowOcclusionHelperWebContents(
        content::WebContents* web_contents,
        OcclusionStateChangedCallback callback)
    : AutoPictureInPictureWindowOcclusionHelperBase(web_contents,
                                                    std::move(callback)),
      content::WebContentsObserver(web_contents) {
  current_visibility_ = web_contents->GetVisibility();
}

AutoPictureInPictureWindowOcclusionHelperWebContents::
    ~AutoPictureInPictureWindowOcclusionHelperWebContents() {
  StopObserving();
}

void AutoPictureInPictureWindowOcclusionHelperWebContents::StartObserving() {
  if (is_observing_) {
    return;
  }
  is_observing_ = true;
  // We are already observing the WebContents, so there is nothing else to do.
}

void AutoPictureInPictureWindowOcclusionHelperWebContents::StopObserving() {
  if (!is_observing_) {
    return;
  }
  is_observing_ = false;
  // We want to continue observing the WebContents, so there is nothing else to
  // do.
}

AutoPictureInPictureWindowOcclusionHelperBase::OcclusionState
AutoPictureInPictureWindowOcclusionHelperWebContents::GetOcclusionState()
    const {
  switch (current_visibility_) {
    case content::Visibility::VISIBLE:
      return OcclusionState::kVisible;
    case content::Visibility::OCCLUDED:
      return OcclusionState::kOccluded;
    case content::Visibility::HIDDEN:
      return OcclusionState::kHidden;
  }
}

void AutoPictureInPictureWindowOcclusionHelperWebContents::OnVisibilityChanged(
    content::Visibility visibility) {
  if (current_visibility_ == visibility) {
    return;
  }
  current_visibility_ = visibility;
  if (is_observing_) {
    RunCallback(GetOcclusionState());
  }
}
