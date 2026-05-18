// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_WINDOW_OCCLUSION_HELPER_WEB_CONTENTS_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_WINDOW_OCCLUSION_HELPER_WEB_CONTENTS_H_

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_window_occlusion_helper_base.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

// This class tracks the window occlusion state of a given WebContents by
// listening for content::WebContentsObserver::OnVisibilityChanged() calls. On
// the platforms where we use this, it detects both occlusion by another Chrome
// window and occlusion by another application's window.
class AutoPictureInPictureWindowOcclusionHelperWebContents
    : public AutoPictureInPictureWindowOcclusionHelperBase,
      public content::WebContentsObserver {
 public:
  AutoPictureInPictureWindowOcclusionHelperWebContents(
      content::WebContents* web_contents,
      OcclusionStateChangedCallback callback);
  AutoPictureInPictureWindowOcclusionHelperWebContents(
      const AutoPictureInPictureWindowOcclusionHelperWebContents&) = delete;
  AutoPictureInPictureWindowOcclusionHelperWebContents& operator=(
      const AutoPictureInPictureWindowOcclusionHelperWebContents&) = delete;
  ~AutoPictureInPictureWindowOcclusionHelperWebContents() override;

  // AutoPictureInPictureWindowOcclusionHelperBase:
  void StartObserving() override;
  void StopObserving() override;
  OcclusionState GetOcclusionState() const override;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  bool is_observing_ = false;
  content::Visibility current_visibility_ = content::Visibility::HIDDEN;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_WINDOW_OCCLUSION_HELPER_WEB_CONTENTS_H_
