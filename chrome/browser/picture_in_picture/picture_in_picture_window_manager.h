// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_

#include "base/memory/singleton.h"

namespace content {
enum class PictureInPictureResult;
class PictureInPictureWindowController;
class WebContents;
}  // namespace content

namespace gfx {
class Size;
}  // namespace gfx

namespace viz {
class SurfaceId;
}  // namespace viz

// PictureInPictureWindowManager is a singleton that handles the lifetime of the
// current Picture-in-Picture window and its PictureInPictureWindowController.
// The class also guarantees that only one window will be present per Chrome
// instances regardless of the number of windows, tabs, profiles, etc.
class PictureInPictureWindowManager {
 public:
  // Returns the singleton instance.
  static PictureInPictureWindowManager* GetInstance();

  // Some PIP windows (e.g. from ARC) may not have a WebContents as the source
  // of the PIP content. This function lets them provide their own window
  // controller directly.
  void EnterPictureInPictureWithController(
      content::PictureInPictureWindowController* pip_window_controller);
  content::PictureInPictureResult EnterPictureInPicture(content::WebContents*,
                                                        const viz::SurfaceId&,
                                                        const gfx::Size&);
  void ExitPictureInPicture();

  content::WebContents* GetWebContents();

 private:
  friend struct base::DefaultSingletonTraits<PictureInPictureWindowManager>;
  class ContentsObserver;

  // Create a Picture-in-Picture window and register it in order to be closed
  // when needed.
  // This is suffixed with "Internal" because `CreateWindow` is part of the
  // Windows API.
  void CreateWindowInternal(content::WebContents*);

  // Closes the active Picture-in-Picture window.
  // There MUST be a window open.
  // This is suffixed with "Internal" to keep consistency with the method above.
  void CloseWindowInternal();

  PictureInPictureWindowManager();
  ~PictureInPictureWindowManager();

  std::unique_ptr<ContentsObserver> contents_observer_;
  content::PictureInPictureWindowController* pip_window_controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureWindowManager);
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_
