// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_error_menu_item_id_provider.h"

#include <bitset>
#include <ostream>

#include "base/lazy_instance.h"
#include "chrome/app/chrome_command_ids.h"

namespace extensions {

namespace {

base::LazyInstance<std::bitset<IDC_EXTENSION_INSTALL_ERROR_LAST -
                               IDC_EXTENSION_INSTALL_ERROR_FIRST +
                               1>>::DestructorAtExit menu_command_ids =
    LAZY_INSTANCE_INITIALIZER;

// Get an available menu ID.
int GetMenuCommandID() {
  int id;
  for (id = IDC_EXTENSION_INSTALL_ERROR_FIRST;
       id <= IDC_EXTENSION_INSTALL_ERROR_LAST; ++id) {
    if (!menu_command_ids.Get()[id - IDC_EXTENSION_INSTALL_ERROR_FIRST]) {
      menu_command_ids.Get().set(id - IDC_EXTENSION_INSTALL_ERROR_FIRST);
      return id;
    }
  }
  // This should not happen.
  DCHECK_LE(id, IDC_EXTENSION_INSTALL_ERROR_LAST)
      << "No available menu command IDs for ExtensionDisabledGlobalError";
  return IDC_EXTENSION_INSTALL_ERROR_LAST;
}

// Make a menu ID available when it is no longer used.
void ReleaseMenuCommandID(int id) {
  menu_command_ids.Get().reset(id - IDC_EXTENSION_INSTALL_ERROR_FIRST);
}

}  // namespace

ExtensionInstallErrorMenuItemIdProvider::
    ExtensionInstallErrorMenuItemIdProvider()
    : menu_command_id_(GetMenuCommandID()) {}

ExtensionInstallErrorMenuItemIdProvider::
    ~ExtensionInstallErrorMenuItemIdProvider() {
  ReleaseMenuCommandID(menu_command_id_);
}

}  // namespace extensions
