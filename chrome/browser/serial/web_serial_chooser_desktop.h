// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERIAL_WEB_SERIAL_CHOOSER_DESKTOP_H_
#define CHROME_BROWSER_SERIAL_WEB_SERIAL_CHOOSER_DESKTOP_H_

#include "base/functional/callback_helpers.h"
#include "chrome/browser/serial/web_serial_chooser.h"

class WebSerialChooserDesktop : public WebSerialChooser {
 public:
  WebSerialChooserDesktop();

  WebSerialChooserDesktop(const WebSerialChooserDesktop&) = delete;
  WebSerialChooserDesktop& operator=(const WebSerialChooserDesktop&) = delete;

  ~WebSerialChooserDesktop() override;

  // WebSerialChooser implementation
  void ShowChooser(
      content::RenderFrameHost* frame,
      std::unique_ptr<SerialChooserController> controller) override;

 private:
  base::ScopedClosureRunner closure_runner_;
};

#endif  // CHROME_BROWSER_SERIAL_WEB_SERIAL_CHOOSER_DESKTOP_H_
