// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/desktop_media_picker_android.h"

#include "chrome/browser/media/android/media_capture_picker_dialog_bridge.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

DesktopMediaPickerAndroid::DesktopMediaPickerAndroid() = default;

DesktopMediaPickerAndroid::~DesktopMediaPickerAndroid() = default;

void DesktopMediaPickerAndroid::Show(
    const DesktopMediaPicker::Params& params,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
    DoneCallback done_callback) {
  CHECK(callback_.is_null());

  DesktopMediaPickerManager::Get()->OnShowDialog(params);
  callback_ = std::move(done_callback);

  // We outlive `dialog_bridge_` so it can take a raw pointer to us.
  dialog_bridge_.Show(
      params.web_contents, params.app_name,
      base::BindOnce(&DesktopMediaPickerAndroid::NotifyDialogResult,
                     base::Unretained(this)));
}

void DesktopMediaPickerAndroid::NotifyDialogResult(
    const content::DesktopMediaID& source) {
  auto callback = std::move(callback_);
  DesktopMediaPickerManager::Get()->OnHideDialog();

  if (callback.is_null()) {
    return;
  }

  // Notify the |callback_| asynchronously because it may need to destroy
  // DesktopMediaPicker.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), source));
}

// static
std::unique_ptr<DesktopMediaPicker> DesktopMediaPicker::Create(
    const content::MediaStreamRequest* request) {
  return std::make_unique<DesktopMediaPickerAndroid>();
}
