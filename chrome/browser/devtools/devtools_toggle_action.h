// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TOGGLE_ACTION_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TOGGLE_ACTION_H_

#include <stddef.h>

#include <memory>
#include <string>


struct DevToolsToggleAction {
 public:
  enum Type {
    kShow,
    kShowConsolePanel,
    kShowElementsPanel,
    kPauseInDebugger,
    kInspect,
    kToggle,
    kReveal,
    kNoOp
  };

  struct RevealParams {
    RevealParams(const std::u16string& url,
                 size_t line_number,
                 size_t column_number);
    ~RevealParams();

    std::u16string url;
    size_t line_number;
    size_t column_number;
  };

  void operator=(const DevToolsToggleAction& rhs);
  DevToolsToggleAction(const DevToolsToggleAction& rhs);
  ~DevToolsToggleAction();

  static DevToolsToggleAction Show();
  static DevToolsToggleAction ShowConsolePanel();
  static DevToolsToggleAction ShowElementsPanel();
  static DevToolsToggleAction PauseInDebugger();
  static DevToolsToggleAction Inspect();
  static DevToolsToggleAction Toggle();
  static DevToolsToggleAction Reveal(const std::u16string& url,
                                     size_t line_number,
                                     size_t column_number);
  static DevToolsToggleAction NoOp();

  Type type() const { return type_; }
  const RevealParams* params() const { return params_.get(); }

 private:
  explicit DevToolsToggleAction(Type type);
  explicit DevToolsToggleAction(RevealParams* reveal_params);

  // The type of action.
  Type type_;

  // Additional parameters for the Reveal action; NULL if of any other type.
  std::unique_ptr<RevealParams> params_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TOGGLE_ACTION_H_
