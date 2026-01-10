// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MCP_SERVER_ACTION_RUNNER_ACTION_RUNNER_H_
#define CHROME_BROWSER_MCP_SERVER_ACTION_RUNNER_ACTION_RUNNER_H_

#include <string>

namespace mcp_server {

// ActionRunner executes UI interactions
// Uses CDP Input domain for native input simulation
class ActionRunner {
 public:
  ActionRunner();
  ~ActionRunner();

  // Click at coordinates
  bool Click(int x, int y);

  // Type text with realistic delays
  bool Type(const std::string& text);

  // Scroll to element or coordinates
  bool Scroll(int x, int y);

  // Drag from start to end
  bool Drag(int x1, int y1, int x2, int y2);

  // Press single key
  bool KeyPress(const std::string& key);

 private:
  // TODO: Implement CDP Input integration
};

}  // namespace mcp_server

#endif  // CHROME_BROWSER_MCP_SERVER_ACTION_RUNNER_ACTION_RUNNER_H_
