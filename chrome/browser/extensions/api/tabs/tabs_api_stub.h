// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_STUB_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_STUB_H_

#include "chrome/common/extensions/api/tabs.h"
#include "extensions/browser/api/execute_code_function.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// This file provides a stub implementation of the chrome.tabs and
// chrome.windows APIs. They are intended for desktop android bringup, as there
// are other APIs (e.g. cookies) that rely types from tabs and windows.

// Windows
class WindowsGetFunction : public ExtensionFunction {
  ~WindowsGetFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.get", WINDOWS_GET)
};
class WindowsGetCurrentFunction : public ExtensionFunction {
  ~WindowsGetCurrentFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getCurrent", WINDOWS_GETCURRENT)
};
class WindowsGetLastFocusedFunction : public ExtensionFunction {
  ~WindowsGetLastFocusedFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getLastFocused", WINDOWS_GETLASTFOCUSED)
};
class WindowsGetAllFunction : public ExtensionFunction {
  ~WindowsGetAllFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getAll", WINDOWS_GETALL)
};
class WindowsCreateFunction : public ExtensionFunction {
  ~WindowsCreateFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.create", WINDOWS_CREATE)
};
class WindowsUpdateFunction : public ExtensionFunction {
  ~WindowsUpdateFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.update", WINDOWS_UPDATE)
};
class WindowsRemoveFunction : public ExtensionFunction {
  ~WindowsRemoveFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.remove", WINDOWS_REMOVE)
};

// Tabs
class TabsGetFunction : public ExtensionFunction {
  ~TabsGetFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.get", TABS_GET)
};
class TabsGetCurrentFunction : public ExtensionFunction {
  ~TabsGetCurrentFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getCurrent", TABS_GETCURRENT)
};
class TabsGetSelectedFunction : public ExtensionFunction {
  ~TabsGetSelectedFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getSelected", TABS_GETSELECTED)
};
class TabsGetAllInWindowFunction : public ExtensionFunction {
  ~TabsGetAllInWindowFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getAllInWindow", TABS_GETALLINWINDOW)
};
class TabsQueryFunction : public ExtensionFunction {
  ~TabsQueryFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.query", TABS_QUERY)
};
class TabsCreateFunction : public ExtensionFunction {
  ~TabsCreateFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.create", TABS_CREATE)
};
class TabsDuplicateFunction : public ExtensionFunction {
  ~TabsDuplicateFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.duplicate", TABS_DUPLICATE)
};
class TabsHighlightFunction : public ExtensionFunction {
  ~TabsHighlightFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.highlight", TABS_HIGHLIGHT)
};
class TabsUpdateFunction : public ExtensionFunction {
 public:
  TabsUpdateFunction();

 private:
  ~TabsUpdateFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.update", TABS_UPDATE)
};
class TabsMoveFunction : public ExtensionFunction {
  ~TabsMoveFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.move", TABS_MOVE)
};
class TabsReloadFunction : public ExtensionFunction {
  ~TabsReloadFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.reload", TABS_RELOAD)
};
class TabsRemoveFunction : public ExtensionFunction {
 public:
  TabsRemoveFunction();

 private:
  ~TabsRemoveFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.remove", TABS_REMOVE)
};
class TabsGroupFunction : public ExtensionFunction {
  ~TabsGroupFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.group", TABS_GROUP)
};
class TabsUngroupFunction : public ExtensionFunction {
  ~TabsUngroupFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.ungroup", TABS_UNGROUP)
};
class TabsDetectLanguageFunction : public ExtensionFunction {
 private:
  ~TabsDetectLanguageFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.detectLanguage", TABS_DETECTLANGUAGE)
};
class TabsCaptureVisibleTabFunction : public ExtensionFunction {
 public:
  TabsCaptureVisibleTabFunction();

 private:
  ~TabsCaptureVisibleTabFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.captureVisibleTab", TABS_CAPTUREVISIBLETAB)
};

// Implement API calls tabs.executeScript, tabs.insertCSS, and tabs.removeCSS.
class ExecuteCodeInTabFunction : public ExecuteCodeFunction {
 public:
  ExecuteCodeInTabFunction();

 protected:
  ~ExecuteCodeInTabFunction() override;

  InitResult Init() override;
  bool ShouldInsertCSS() const override;
  bool ShouldRemoveCSS() const override;
  bool CanExecuteScriptOnPage(std::string* error) override;
  ScriptExecutor* GetScriptExecutor(std::string* error) override;
  bool IsWebView() const override;
  int GetRootFrameId() const override;
  const GURL& GetWebViewSrc() const override;
};

class TabsExecuteScriptFunction : public ExecuteCodeInTabFunction {
 private:
  ~TabsExecuteScriptFunction() override = default;

  DECLARE_EXTENSION_FUNCTION("tabs.executeScript", TABS_EXECUTESCRIPT)
};

class TabsInsertCSSFunction : public ExecuteCodeInTabFunction {
 private:
  ~TabsInsertCSSFunction() override = default;

  bool ShouldInsertCSS() const override;

  DECLARE_EXTENSION_FUNCTION("tabs.insertCSS", TABS_INSERTCSS)
};

class TabsRemoveCSSFunction : public ExecuteCodeInTabFunction {
 private:
  ~TabsRemoveCSSFunction() override = default;

  bool ShouldRemoveCSS() const override;

  DECLARE_EXTENSION_FUNCTION("tabs.removeCSS", TABS_REMOVECSS)
};

class TabsSetZoomFunction : public ExtensionFunction {
 private:
  ~TabsSetZoomFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.setZoom", TABS_SETZOOM)
};

class TabsGetZoomFunction : public ExtensionFunction {
 private:
  ~TabsGetZoomFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getZoom", TABS_GETZOOM)
};

class TabsSetZoomSettingsFunction : public ExtensionFunction {
 private:
  ~TabsSetZoomSettingsFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.setZoomSettings", TABS_SETZOOMSETTINGS)
};

class TabsGetZoomSettingsFunction : public ExtensionFunction {
 private:
  ~TabsGetZoomSettingsFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getZoomSettings", TABS_GETZOOMSETTINGS)
};

class TabsDiscardFunction : public ExtensionFunction {
 public:
  TabsDiscardFunction();

 private:
  ~TabsDiscardFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.discard", TABS_DISCARD)
};

class TabsGoForwardFunction : public ExtensionFunction {
 public:
  TabsGoForwardFunction() {}

 private:
  ~TabsGoForwardFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.goForward", TABS_GOFORWARD)
};

class TabsGoBackFunction : public ExtensionFunction {
 public:
  TabsGoBackFunction() {}

 private:
  ~TabsGoBackFunction() override = default;
  ExtensionFunction::ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.goBack", TABS_GOBACK)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_STUB_H_
