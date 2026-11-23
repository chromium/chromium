// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SIDEBAR_SIDEBAR_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SIDEBAR_SIDEBAR_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

// Implements `chrome.sidebar.getLayout()`.
class SidebarGetLayoutFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sidebar.getLayout", SIDEBAR_GETLAYOUT)
  SidebarGetLayoutFunction();
  SidebarGetLayoutFunction(const SidebarGetLayoutFunction&) = delete;
  SidebarGetLayoutFunction& operator=(const SidebarGetLayoutFunction&) = delete;

 private:
  ~SidebarGetLayoutFunction() override;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SIDEBAR_SIDEBAR_API_H_
