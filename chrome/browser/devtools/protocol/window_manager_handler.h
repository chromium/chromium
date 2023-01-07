// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_WINDOW_MANAGER_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_WINDOW_MANAGER_HANDLER_H_

#include "chrome/browser/devtools/protocol/window_manager.h"

class WindowManagerHandler : public protocol::WindowManager::Backend {
 public:
  explicit WindowManagerHandler(protocol::UberDispatcher* dispatcher);

  WindowManagerHandler(const WindowManagerHandler&) = delete;
  WindowManagerHandler& operator=(const WindowManagerHandler&) = delete;

  ~WindowManagerHandler() override;

  // WindowManager::Backend:
  protocol::Response EnterOverviewMode() override;
  protocol::Response ExitOverviewMode() override;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_WINDOW_MANAGER_HANDLER_H_
