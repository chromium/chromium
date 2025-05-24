// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERIAL_WEB_SERIAL_CHOOSER_H_
#define CHROME_BROWSER_SERIAL_WEB_SERIAL_CHOOSER_H_

#include <memory>

#include "content/public/browser/serial_chooser.h"

namespace content {
class RenderFrameHost;
}

class SerialChooserController;

class WebSerialChooser : public content::SerialChooser {
 public:
  static std::unique_ptr<WebSerialChooser> Create(
      content::RenderFrameHost* frame,
      std::unique_ptr<SerialChooserController> controller);

  WebSerialChooser(const WebSerialChooser&) = delete;
  WebSerialChooser& operator=(const WebSerialChooser&) = delete;

 protected:
  WebSerialChooser();

  virtual void ShowChooser(
      content::RenderFrameHost* frame,
      std::unique_ptr<SerialChooserController> controller) = 0;
};

#endif  // CHROME_BROWSER_SERIAL_WEB_SERIAL_CHOOSER_H_
