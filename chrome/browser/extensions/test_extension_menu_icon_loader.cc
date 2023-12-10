// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_menu_icon_loader.h"

namespace extensions {

void TestExtensionMenuIconLoader::LoadIcon(
    content::BrowserContext* context,
    const Extension* extension,
    const MenuItem::ExtensionKey& extension_key) {
  load_icon_calls_++;
  ExtensionMenuIconLoader::LoadIcon(context, extension, extension_key);
}

gfx::Image TestExtensionMenuIconLoader::GetIcon(
    const MenuItem::ExtensionKey& extension_key) {
  get_icon_calls_++;
  return ExtensionMenuIconLoader::GetIcon(extension_key);
}

void TestExtensionMenuIconLoader::RemoveIcon(
    const MenuItem::ExtensionKey& extension_key) {
  remove_icon_calls_++;
  return ExtensionMenuIconLoader::RemoveIcon(extension_key);
}

void TestExtensionMenuIconLoader::Reset() {
  load_icon_calls_ = 0;
  get_icon_calls_ = 0;
  remove_icon_calls_ = 0;
}

}  // namespace extensions
