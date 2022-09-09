// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_toggle_action.h"

#include <memory>

DevToolsToggleAction::RevealParams::RevealParams(const std::u16string& url,
                                                 size_t line_number,
                                                 size_t column_number)
    : url(url), line_number(line_number), column_number(column_number) {}

DevToolsToggleAction::RevealParams::~RevealParams() {
}

DevToolsToggleAction::DevToolsToggleAction(Type type) : type_(type) {
}

DevToolsToggleAction::DevToolsToggleAction(RevealParams* params)
    : type_(kReveal), params_(params) {
}

DevToolsToggleAction::DevToolsToggleAction(const DevToolsToggleAction& rhs)
    : type_(rhs.type_),
      params_(rhs.params_.get() ? new RevealParams(*rhs.params_) : nullptr) {}

void DevToolsToggleAction::operator=(const DevToolsToggleAction& rhs) {
  type_ = rhs.type_;
  if (rhs.params_.get())
    params_ = std::make_unique<RevealParams>(*rhs.params_);
}

DevToolsToggleAction::~DevToolsToggleAction() {
}

// static
DevToolsToggleAction DevToolsToggleAction::Show() {
  return DevToolsToggleAction(kShow);
}

// static
DevToolsToggleAction DevToolsToggleAction::ShowConsolePanel() {
  return DevToolsToggleAction(kShowConsolePanel);
}

// static
DevToolsToggleAction DevToolsToggleAction::ShowElementsPanel() {
  return DevToolsToggleAction(kShowElementsPanel);
}

// static
DevToolsToggleAction DevToolsToggleAction::PauseInDebugger() {
  return DevToolsToggleAction(kPauseInDebugger);
}

// static
DevToolsToggleAction DevToolsToggleAction::Inspect() {
  return DevToolsToggleAction(kInspect);
}

// static
DevToolsToggleAction DevToolsToggleAction::Toggle() {
  return DevToolsToggleAction(kToggle);
}

// static
DevToolsToggleAction DevToolsToggleAction::Reveal(const std::u16string& url,
                                                  size_t line_number,
                                                  size_t column_number) {
  return DevToolsToggleAction(
      new RevealParams(url, line_number, column_number));
}

// static
DevToolsToggleAction DevToolsToggleAction::NoOp() {
  return DevToolsToggleAction(kNoOp);
}
