// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_PIP_ARC_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_PIP_ARC_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class WebContents;

}  // namespace content

namespace arc {

class ArcPipBridge;

// Implementation of PictureInPictureWindowController for ARC. This does nothing
// for most of the methods, as most of it is managed on the Android side.
class ArcPictureInPictureWindowControllerImpl
    : public content::PictureInPictureWindowController {
 public:
  explicit ArcPictureInPictureWindowControllerImpl(
      arc::ArcPipBridge* arc_pip_bridge);

  ArcPictureInPictureWindowControllerImpl(
      const ArcPictureInPictureWindowControllerImpl&) = delete;
  ArcPictureInPictureWindowControllerImpl& operator=(
      const ArcPictureInPictureWindowControllerImpl&) = delete;

  ~ArcPictureInPictureWindowControllerImpl() override;

  // PictureInPictureWindowController:
  void Show() override;
  void FocusInitiator() override;
  void Close(bool should_pause_video) override;
  void CloseAndFocusInitiator() override;
  void OnWindowDestroyed(bool should_pause_video) override;
  content::WebContents* GetWebContents() override;
  std::optional<gfx::Rect> GetWindowBounds() override;
  content::WebContents* GetChildWebContents() override;
  std::optional<url::Origin> GetOrigin() override;

 private:
  const raw_ptr<arc::ArcPipBridge> arc_pip_bridge_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_PIP_ARC_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
