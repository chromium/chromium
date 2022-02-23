// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"

namespace content {
enum class PictureInPictureResult;
class PictureInPictureWindowController;
class WebContents;
}  // namespace content

// PictureInPictureWindowManager is a singleton that handles the lifetime of the
// current Picture-in-Picture window and its PictureInPictureWindowController.
// The class also guarantees that only one window will be present per Chrome
// instances regardless of the number of windows, tabs, profiles, etc.
class PictureInPictureWindowManager {
 public:
  // Returns the singleton instance.
  static PictureInPictureWindowManager* GetInstance();

  PictureInPictureWindowManager(const PictureInPictureWindowManager&) = delete;
  PictureInPictureWindowManager& operator=(
      const PictureInPictureWindowManager&) = delete;

  // Shows a PIP window using the window controller for a video element.
  //
  // This mode is triggered through WebContentsDelegate::EnterPictureInPicture,
  // and the default implementation of that fails with a kNotSupported
  // result. For compatibility, this method must also return a
  // content::PictureInPictureResult even though it doesn't fail.
  content::PictureInPictureResult EnterVideoPictureInPicture(
      content::WebContents*);

  // Shows a PIP window using the window controller for document picture in
  // picture.
  //
  // Document picture-in-picture mode is triggered from the Renderer via
  // WindowOpenDisposition::NEW_PICTURE_IN_PICTURE, and the browser
  // (i.e. Chrome's BrowserNavigator) then calls this method to create the
  // window. There's no corresponding path through the WebContentsDelegate, so
  // it doesn't have a failure state.
  void EnterDocumentPictureInPicture(
      content::WebContents* parent_web_contents,
      std::unique_ptr<content::WebContents> child_web_contents);

  // Shows a PIP window with an explicitly provided window controller. This is
  // used by ChromeOS ARC windows which do not have a WebContents as the source.
  void EnterPictureInPictureWithController(
      content::PictureInPictureWindowController* pip_window_controller);

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
  raw_ptr<content::PictureInPictureWindowController> pip_window_controller_ =
      nullptr;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_
