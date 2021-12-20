// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_H_

#include "chrome/browser/download/bubble/download_icon_state.h"

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
  // Updates the download icon according to |state|.
  virtual void UpdateDownloadIcon(download::DownloadIconState state) = 0;

 protected:
  virtual ~DownloadDisplay();
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_H_
