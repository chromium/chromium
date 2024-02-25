// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_menu_icon_loader.h"

#include "chrome/browser/extensions/menu_manager.h"
#include "extensions/common/extension.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace extensions {

void TestExtensionMenuIconLoader::LoadIcon(
    content::BrowserContext* context,
    const Extension* extension,
    const MenuItem::ExtensionKey& extension_key) {
  load_icon_calls_++;
  if (extension) {
    ExtensionMenuIconLoader::LoadIcon(context, extension, extension_key);
  }
}

gfx::Image TestExtensionMenuIconLoader::GetIcon(
    const MenuItem::ExtensionKey& extension_key) {
  get_icon_calls_++;
  if (extension_key.extension_id.empty()) {
    return gfx::test::CreateImage(gfx::kFaviconSize);
  }
  return ExtensionMenuIconLoader::GetIcon(extension_key);
}

void TestExtensionMenuIconLoader::RemoveIcon(
    const MenuItem::ExtensionKey& extension_key) {
  remove_icon_calls_++;
  if (!extension_key.extension_id.empty()) {
    ExtensionMenuIconLoader::RemoveIcon(extension_key);
  }
}

void TestExtensionMenuIconLoader::Reset() {
  load_icon_calls_ = 0;
  get_icon_calls_ = 0;
  remove_icon_calls_ = 0;
}

}  // namespace extensions
