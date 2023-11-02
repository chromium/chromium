// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_ERROR_MENU_ITEM_ID_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_ERROR_MENU_ITEM_ID_PROVIDER_H_

namespace extensions {

// Helper class to generate not-in-use and unique menu item id for GlobalErrors
// that require items in hotdog menu.
//
// The generated ids lie within the interval:
// [IDC_EXTENSION_INSTALL_ERROR_FIRST, IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST].
class ExtensionInstallErrorMenuItemIdProvider {
 public:
  ExtensionInstallErrorMenuItemIdProvider();

  ExtensionInstallErrorMenuItemIdProvider(
      const ExtensionInstallErrorMenuItemIdProvider&) = delete;
  ExtensionInstallErrorMenuItemIdProvider& operator=(
      const ExtensionInstallErrorMenuItemIdProvider&) = delete;

  ~ExtensionInstallErrorMenuItemIdProvider();

  int menu_command_id() { return menu_command_id_; }

 private:
  int menu_command_id_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_ERROR_MENU_ITEM_ID_PROVIDER_H_
