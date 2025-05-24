// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_DESKTOP_MEDIA_PICKER_ANDROID_H_
#define CHROME_BROWSER_MEDIA_ANDROID_DESKTOP_MEDIA_PICKER_ANDROID_H_

#include "chrome/browser/media/android/media_capture_picker_dialog_bridge.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"

class MediaCapturePickerDialogBridge;

// Implementation of DesktopMediaPicker for Android.
class DesktopMediaPickerAndroid : public DesktopMediaPicker {
 public:
  DesktopMediaPickerAndroid();
  DesktopMediaPickerAndroid(const DesktopMediaPickerAndroid&) = delete;
  DesktopMediaPickerAndroid& operator=(const DesktopMediaPickerAndroid&) =
      delete;
  DesktopMediaPickerAndroid(DesktopMediaPickerAndroid&&) = delete;
  DesktopMediaPickerAndroid& operator=(DesktopMediaPickerAndroid&&) = delete;
  ~DesktopMediaPickerAndroid() override;

  void NotifyDialogResult(const content::DesktopMediaID& source);

  // DesktopMediaPicker:
  void Show(const DesktopMediaPicker::Params& params,
            std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
            DoneCallback done_callback) override;

 private:
  MediaCapturePickerDialogBridge dialog_bridge_;
  DoneCallback callback_;
};

#endif  // CHROME_BROWSER_MEDIA_ANDROID_DESKTOP_MEDIA_PICKER_ANDROID_H_
