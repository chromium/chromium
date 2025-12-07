// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERIAL_ANDROID_WEB_SERIAL_CHOOSER_ANDROID_H_
#define CHROME_BROWSER_SERIAL_ANDROID_WEB_SERIAL_CHOOSER_ANDROID_H_

#include <memory>

#include "chrome/browser/serial/web_serial_chooser.h"

class SerialChooserDialogAndroid;

class WebSerialChooserAndroid : public WebSerialChooser {
 public:
  WebSerialChooserAndroid();

  WebSerialChooserAndroid(const WebSerialChooserAndroid&) = delete;
  WebSerialChooserAndroid& operator=(const WebSerialChooserAndroid&) = delete;

  ~WebSerialChooserAndroid() override;

  // WebSerialChooser implementation
  void ShowChooser(
      content::RenderFrameHost* frame,
      std::unique_ptr<SerialChooserController> controller) override;

 private:
  void OnDialogClosed();

  std::unique_ptr<SerialChooserDialogAndroid> dialog_;
};

#endif  // CHROME_BROWSER_SERIAL_ANDROID_WEB_SERIAL_CHOOSER_ANDROID_H_
