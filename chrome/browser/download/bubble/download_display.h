// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_H_

namespace offline_items_collection {
struct ContentId;
}

class DownloadDisplay {
 public:
  // Shows the download display.
  virtual void Show() = 0;
  // Hides the download display.
  virtual void Hide() = 0;
  // Returns whether or not the download display is visible.
  virtual bool IsShowing() = 0;
  // Enables potential actions resulting from clicking the download display.
  virtual void Enable() = 0;
  // Disables potential actions resulting from clicking the download display.
  virtual void Disable() = 0;
  // Updates the download icon.
  // If |show_animation| is true, an animated icon will be shown.
  virtual void UpdateDownloadIcon(bool show_animation) = 0;
  // Shows detailed information on the download display. It can be a popup or
  // dialog or partial view, essentially anything other than the main view.
  virtual void ShowDetails() = 0;
  // Hide the detailed information on the download display.
  virtual void HideDetails() = 0;
  // Returns whether the details are visible.
  virtual bool IsShowingDetails() = 0;
  // Returns whether it is currently in fullscreen and the view that hosts the
  // download display is hidden.
  virtual bool IsFullscreenWithParentViewHidden() = 0;
  // Whether we should show the exclusive access bubble upon starting a download
  // in fullscreen mode. If the user cannot exit fullscreen, there is no point
  // in showing an exclusive access bubble telling the user to exit fullscreen
  // to view their downloads, because exiting is impossible. If we are in
  // immersive fullscreen mode, we don't need to show the exclusive access
  // bubble because we will just temporarily reveal the toolbar when the
  // downloads finish.
  virtual bool ShouldShowExclusiveAccessBubble() = 0;
  // Open the security subpage for the download with `id`, if it exists.
  virtual void OpenSecuritySubpage(
      const offline_items_collection::ContentId& id) = 0;

 protected:
  virtual ~DownloadDisplay();
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_H_
