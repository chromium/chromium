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

class SidePanelSetPanelBehaviorFunction : public SidePanelApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sidePanel.setPanelBehavior",
                             SIDEPANEL_SETPANELBEHAVIOR)
  SidePanelSetPanelBehaviorFunction() = default;
  SidePanelSetPanelBehaviorFunction(const SidePanelSetPanelBehaviorFunction&) =
      delete;
  SidePanelSetPanelBehaviorFunction& operator=(
      const SidePanelSetPanelBehaviorFunction&) = delete;

 private:
  ~SidePanelSetPanelBehaviorFunction() override = default;
  ResponseAction RunFunction() override;
};

class SidePanelGetPanelBehaviorFunction : public SidePanelApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sidePanel.getPanelBehavior",
                             SIDEPANEL_GETPANELBEHAVIOR)
  SidePanelGetPanelBehaviorFunction() = default;
  SidePanelGetPanelBehaviorFunction(const SidePanelGetPanelBehaviorFunction&) =
      delete;
  SidePanelGetPanelBehaviorFunction& operator=(
      const SidePanelGetPanelBehaviorFunction&) = delete;

 private:
  ~SidePanelGetPanelBehaviorFunction() override = default;
  ResponseAction RunFunction() override;
};

class SidePanelOpenFunction : public SidePanelApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sidePanel.open", SIDEPANEL_OPEN)
  SidePanelOpenFunction() = default;
  SidePanelOpenFunction(const SidePanelOpenFunction&) = delete;
  SidePanelOpenFunction& operator=(const SidePanelOpenFunction&) = delete;

 private:
  ~SidePanelOpenFunction() override = default;
  ResponseAction RunFunction() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SIDE_PANEL_SIDE_PANEL_API_H_
