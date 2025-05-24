// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/android/web_serial_chooser_android.h"

#include "chrome/browser/ui/android/device_dialog/serial_chooser_dialog_android.h"
#include "chrome/browser/ui/serial/serial_chooser_controller.h"

WebSerialChooserAndroid::WebSerialChooserAndroid() = default;

WebSerialChooserAndroid::~WebSerialChooserAndroid() = default;

void WebSerialChooserAndroid::ShowChooser(
    content::RenderFrameHost* frame,
    std::unique_ptr<SerialChooserController> controller) {
  dialog_ = SerialChooserDialogAndroid::Create(
      frame, std::move(controller),
      base::BindOnce(&WebSerialChooserAndroid::OnDialogClosed,
                     base::Unretained(this)));
}

void WebSerialChooserAndroid::OnDialogClosed() {
  dialog_.reset();
}
