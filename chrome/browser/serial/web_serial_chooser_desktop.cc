// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/web_serial_chooser_desktop.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/serial/serial_chooser_controller.h"

WebSerialChooserDesktop::WebSerialChooserDesktop() = default;

WebSerialChooserDesktop::~WebSerialChooserDesktop() = default;

void WebSerialChooserDesktop::ShowChooser(
    content::RenderFrameHost* frame,
    std::unique_ptr<SerialChooserController> controller) {
  closure_runner_.ReplaceClosure(
      chrome::ShowDeviceChooserDialog(frame, std::move(controller)));
}
