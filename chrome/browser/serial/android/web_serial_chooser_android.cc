// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/android/web_serial_chooser_android.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/serial/serial_chooser_controller.h"

WebSerialChooserAndroid::WebSerialChooserAndroid() = default;

WebSerialChooserAndroid::~WebSerialChooserAndroid() = default;

void WebSerialChooserAndroid::ShowChooser(
    content::RenderFrameHost* frame,
    std::unique_ptr<SerialChooserController> controller) {
  // TODO(crbug.com/380129064): Add serial port chooser for Android.
  NOTIMPLEMENTED();
}
