// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/web_serial_chooser.h"

#include "chrome/browser/ui/serial/serial_chooser_controller.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/serial/android/web_serial_chooser_android.h"
#else
#include "chrome/browser/serial/web_serial_chooser_desktop.h"
#endif

std::unique_ptr<WebSerialChooser> WebSerialChooser::Create(
    content::RenderFrameHost* frame,
    std::unique_ptr<SerialChooserController> controller) {
  std::unique_ptr<WebSerialChooser> chooser;
#if BUILDFLAG(IS_ANDROID)
  chooser = std::make_unique<WebSerialChooserAndroid>();
#else
  chooser = std::make_unique<WebSerialChooserDesktop>();
#endif
  chooser->ShowChooser(frame, std::move(controller));
  return chooser;
}

WebSerialChooser::WebSerialChooser() = default;
