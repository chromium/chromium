// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_ANDROID_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_ANDROID_H_

#include "chrome/browser/devtools/protocol/browser.h"

class BrowserHandlerAndroid : public protocol::Browser::Backend {
 public:
  BrowserHandlerAndroid(protocol::UberDispatcher* dispatcher,
                        const std::string& target_id);

  BrowserHandlerAndroid(const BrowserHandlerAndroid&) = delete;
  BrowserHandlerAndroid& operator=(const BrowserHandlerAndroid&) = delete;

  ~BrowserHandlerAndroid() override;

  // Browser::Backend:
  protocol::Response GetWindowForTarget(
      std::optional<std::string> target_id,
      int* out_window_id,
      std::unique_ptr<protocol::Browser::Bounds>* out_bounds) override;
  protocol::Response GetWindowBounds(
      int window_id,
      std::unique_ptr<protocol::Browser::Bounds>* out_bounds) override;
  protocol::Response Close() override;
  protocol::Response SetWindowBounds(
      int window_id,
      std::unique_ptr<protocol::Browser::Bounds> window_bounds) override;
  protocol::Response SetContentsSize(int window_id,
                                     std::optional<int> width,
                                     std::optional<int> height) override;
  protocol::Response SetDockTile(
      std::optional<std::string> label,
      std::optional<protocol::Binary> image) override;
  protocol::Response ExecuteBrowserCommand(
      const protocol::Browser::BrowserCommandId& command_id) override;
  protocol::Response AddPrivacySandboxEnrollmentOverride(
      const std::string& in_url) override;

 private:
  const std::string target_id_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_ANDROID_H_
