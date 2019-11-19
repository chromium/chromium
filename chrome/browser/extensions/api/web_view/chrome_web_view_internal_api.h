// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_VIEW_CHROME_WEB_VIEW_INTERNAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_VIEW_CHROME_WEB_VIEW_INTERNAL_API_H_

#include "base/macros.h"
#include "extensions/browser/api/guest_view/web_view/web_view_internal_api.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

// WARNING: *WebViewInternal could be loaded in an unblessed context, thus any
// new APIs must extend WebViewInternalExtensionFunction or
// WebViewInternalExecuteCodeFunction which do a process ID check to prevent
// abuse by normal renderer processes.
namespace extensions {

class ChromeWebViewInternalContextMenusCreateFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("chromeWebViewInternal.contextMenusCreate",
                             WEBVIEWINTERNAL_CONTEXTMENUSCREATE)
  ChromeWebViewInternalContextMenusCreateFunction() {}

 protected:
  ~ChromeWebViewInternalContextMenusCreateFunction() override {}

  // ExtensionFunction implementation.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeWebViewInternalContextMenusCreateFunction);
};

class ChromeWebViewInternalContextMenusUpdateFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("chromeWebViewInternal.contextMenusUpdate",
                             WEBVIEWINTERNAL_CONTEXTMENUSUPDATE)
  ChromeWebViewInternalContextMenusUpdateFunction() {}

 protected:
  ~ChromeWebViewInternalContextMenusUpdateFunction() override {}

  // ExtensionFunction implementation.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeWebViewInternalContextMenusUpdateFunction);
};

class ChromeWebViewInternalContextMenusRemoveFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("chromeWebViewInternal.contextMenusRemove",
                             WEBVIEWINTERNAL_CONTEXTMENUSREMOVE)
  ChromeWebViewInternalContextMenusRemoveFunction() {}

 protected:
  ~ChromeWebViewInternalContextMenusRemoveFunction() override {}

  // ExtensionFunction implementation.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeWebViewInternalContextMenusRemoveFunction);
};

class ChromeWebViewInternalContextMenusRemoveAllFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("chromeWebViewInternal.contextMenusRemoveAll",
                             WEBVIEWINTERNAL_CONTEXTMENUSREMOVEALL)
  ChromeWebViewInternalContextMenusRemoveAllFunction() {}

 protected:
  ~ChromeWebViewInternalContextMenusRemoveAllFunction() override {}

  // ExtensionFunction implementation.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeWebViewInternalContextMenusRemoveAllFunction);
};

class ChromeWebViewInternalShowContextMenuFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("chromeWebViewInternal.showContextMenu",
                             WEBVIEWINTERNAL_SHOWCONTEXTMENU)

  ChromeWebViewInternalShowContextMenuFunction();

 protected:
  ~ChromeWebViewInternalShowContextMenuFunction() override;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebViewInternalShowContextMenuFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_VIEW_CHROME_WEB_VIEW_INTERNAL_API_H_
