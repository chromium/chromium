// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usb/web_usb_chooser_android.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/ui/android/device_dialog/usb_chooser_dialog_android.h"
#include "chrome/browser/usb/usb_chooser_controller.h"

WebUsbChooserAndroid::WebUsbChooserAndroid(
    content::RenderFrameHost* render_frame_host)
    : WebUsbChooser(render_frame_host) {}

WebUsbChooserAndroid::~WebUsbChooserAndroid() {}

void WebUsbChooserAndroid::ShowChooser(
    std::unique_ptr<UsbChooserController> controller) {
  dialog_ = UsbChooserDialogAndroid::Create(
      render_frame_host(), std::move(controller),
      base::BindOnce(&WebUsbChooserAndroid::OnDialogClosed,
                     base::Unretained(this)));
}

void WebUsbChooserAndroid::OnDialogClosed() {
  dialog_.reset();
}

base::WeakPtr<WebUsbChooser> WebUsbChooserAndroid::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
