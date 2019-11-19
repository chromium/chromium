// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_CAPTION_SETTINGS_DIALOG_H_
#define CHROME_BROWSER_ACCESSIBILITY_CAPTION_SETTINGS_DIALOG_H_

#include "base/macros.h"

namespace captions {

// An abstraction of a caption settings dialog. This is used for the captions
// sub-section of Settings.
class CaptionSettingsDialog {
 public:
  // Displays the native captions manager dialog.
  static void ShowCaptionSettingsDialog();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptionSettingsDialog);
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_CAPTION_SETTINGS_DIALOG_H_
