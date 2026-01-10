// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/action_runner/action_runner.h"
#include "base/logging.h"

namespace mcp_server {

ActionRunner::ActionRunner() {}
ActionRunner::~ActionRunner() = default;

bool ActionRunner::Click(int x, int y) {
  LOG(INFO) << "Click at (" << x << ", " << y << ")";
  // TODO: Use CDP Input.dispatchMouseEvent
  return true;
}

bool ActionRunner::Type(const std::string& text) {
  LOG(INFO) << "Type: " << text;
  // TODO: Use CDP Input.dispatchKeyEvent with realistic delays
  return true;
}

bool ActionRunner::Scroll(int x, int y) {
  LOG(INFO) << "Scroll to (" << x << ", " << y << ")";
  // TODO: Use scrollIntoView or JS execution
  return true;
}

bool ActionRunner::Drag(int x1, int y1, int x2, int y2) {
  LOG(INFO) << "Drag from (" << x1 << "," << y1 << ") to (" << x2 << "," << y2 << ")";
  // TODO: Simulate mouse down, move, up events
  return true;
}

bool ActionRunner::KeyPress(const std::string& key) {
  LOG(INFO) << "KeyPress: " << key;
  // TODO: Use CDP Input.dispatchKeyEvent
  return true;
}

}  // namespace mcp_server
