// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class SidePanelService;

class SidePanelApiFunction : public ExtensionFunction {
 protected:
  SidePanelApiFunction();
  ~SidePanelApiFunction() override;
  ResponseAction Run() override;

  virtual ResponseAction RunFunction() = 0;
  SidePanelService* GetService();
};

class SidePanelGetOptionsFunction : public SidePanelApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sidePanel.getOptions", SIDEPANEL_GETOPTIONS)
  SidePanelGetOptionsFunction() = default;
  SidePanelGetOptionsFunction(const SidePanelGetOptionsFunction&) = delete;
  SidePanelGetOptionsFunction& operator=(const SidePanelGetOptionsFunction&) =
      delete;

 private:
  ~SidePanelGetOptionsFunction() override = default;
  ResponseAction RunFunction() override;
};

class SidePanelSetOptionsFunction : public SidePanelApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sidePanel.setOptions", SIDEPANEL_SETOPTIONS)
  SidePanelSetOptionsFunction() = default;
  SidePanelSetOptionsFunction(const SidePanelSetOptionsFunction&) = delete;
  SidePanelSetOptionsFunction& operator=(const SidePanelSetOptionsFunction&) =
      delete;

 private:
  ~SidePanelSetOptionsFunction() override = default;
  ResponseAction RunFunction() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_API_H_
