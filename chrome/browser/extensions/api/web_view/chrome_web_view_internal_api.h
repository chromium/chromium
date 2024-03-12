// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_VIEW_CHROME_WEB_VIEW_INTERNAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_VIEW_CHROME_WEB_VIEW_INTERNAL_API_H_

#include "extensions/browser/api/guest_view/web_view/web_view_internal_api.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

// WARNING: *WebViewInternal could be loaded in an unprivileged context, thus
// any new APIs must extend WebViewInternalExtensionFunction or
// WebViewInternalExecuteCodeFunction which do a process ID check to prevent
// abuse by normal renderer processes.
namespace extensions {

class ChromeWebViewInternalContextMenusCreateFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("chromeWebViewInternal.contextMenusCreate",
                             WEBVIEWINTERNAL_CONTEXTMENUSCREATE)
  ChromeWebViewInternalContextMenusCreateFunction() {}

  ChromeWebViewInternalContextMenusCreateFunction(
      const ChromeWebViewInternalContextMenusCreateFunction&) = delete;
  ChromeWebViewInternalContextMenusCreateFunction& operator=(
      const ChromeWebViewInternalContextMenusCreateFunction&) = delete;

 protected:
  ~ChromeWebViewInternalContextMenusCreateFunction() override {}

  // ExtensionFunction implementation.
  ResponseAction Run() override;
};

class ChromeWebViewInternalContextMenusUpdateFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("chromeWebViewInternal.contextMenusUpdate",
                             WEBVIEWINTERNAL_CONTEXTMENUSUPDATE)
  ChromeWebViewInternalContextMenusUpdateFunction() {}

  ChromeWebViewInternalContextMenusUpdateFunction(
      const ChromeWebViewInternalContextMenusUpdateFunction&) = delete;
  ChromeWebViewInternalContextMenusUpdateFunction& operator=(
      const ChromeWebViewInternalContextMenusUpdateFunction&) = delete;

 protected:
  ~ChromeWebViewInternalContextMenusUpdateFunction() override {}

  // ExtensionFunction implementation.
  ResponseAction Run() override;
};

class ChromeWebViewInternalContextMenusRemoveFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("chromeWebViewInternal.contextMenusRemove",
                             WEBVIEWINTERNAL_CONTEXTMENUSREMOVE)
  ChromeWebViewInternalContextMenusRemoveFunction() {}

  ChromeWebViewInternalContextMenusRemoveFunction(
      const ChromeWebViewInternalContextMenusRemoveFunction&) = delete;
  ChromeWebViewInternalContextMenusRemoveFunction& operator=(
      const ChromeWebViewInternalContextMenusRemoveFunction&) = delete;

 protected:
  ~ChromeWebViewInternalContextMenusRemoveFunction() override {}

  // ExtensionFunction implementation.
  ResponseAction Run() override;
};

class ChromeWebViewInternalContextMenusRemoveAllFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("chromeWebViewInternal.contextMenusRemoveAll",
                             WEBVIEWINTERNAL_CONTEXTMENUSREMOVEALL)
  ChromeWebViewInternalContextMenusRemoveAllFunction() {}

  ChromeWebViewInternalContextMenusRemoveAllFunction(
      const ChromeWebViewInternalContextMenusRemoveAllFunction&) = delete;
  ChromeWebViewInternalContextMenusRemoveAllFunction& operator=(
      const ChromeWebViewInternalContextMenusRemoveAllFunction&) = delete;

 protected:
  ~ChromeWebViewInternalContextMenusRemoveAllFunction() override {}

  // ExtensionFunction implementation.
  ResponseAction Run() override;
};

class ChromeWebViewInternalShowContextMenuFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("chromeWebViewInternal.showContextMenu",
                             WEBVIEWINTERNAL_SHOWCONTEXTMENU)

  ChromeWebViewInternalShowContextMenuFunction();

  ChromeWebViewInternalShowContextMenuFunction(
      const ChromeWebViewInternalShowContextMenuFunction&) = delete;
  ChromeWebViewInternalShowContextMenuFunction& operator=(
      const ChromeWebViewInternalShowContextMenuFunction&) = delete;

 protected:
  ~ChromeWebViewInternalShowContextMenuFunction() override;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_VIEW_CHROME_WEB_VIEW_INTERNAL_API_H_
