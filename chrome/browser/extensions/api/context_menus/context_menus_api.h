// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class ContextMenusCreateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.create", CONTEXTMENUS_CREATE)

 protected:
  ~ContextMenusCreateFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ContextMenusUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.update", CONTEXTMENUS_UPDATE)

 protected:
  ~ContextMenusUpdateFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ContextMenusRemoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.remove", CONTEXTMENUS_REMOVE)

 protected:
  ~ContextMenusRemoveFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ContextMenusRemoveAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.removeAll", CONTEXTMENUS_REMOVEALL)

 protected:
  ~ContextMenusRemoveAllFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_H_
