// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_CAPTION_SETTINGS_DIALOG_H_
#define CHROME_BROWSER_ACCESSIBILITY_CAPTION_SETTINGS_DIALOG_H_

namespace captions {

// An abstraction of a caption settings dialog. This is used for the captions
// sub-section of Settings.
class CaptionSettingsDialog {
 public:
  CaptionSettingsDialog() = delete;
  CaptionSettingsDialog(const CaptionSettingsDialog&) = delete;
  CaptionSettingsDialog& operator=(const CaptionSettingsDialog&) = delete;

  // Displays the native captions manager dialog.
  static void ShowCaptionSettingsDialog();
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_CAPTION_SETTINGS_DIALOG_H_
