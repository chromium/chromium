// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_menu_icon_loader.h"

#include "content/public/browser/browser_context.h"
#include "extensions/common/extension.h"
#include "ui/gfx/image/image.h"

namespace extensions {

ExtensionMenuIconLoader::ExtensionMenuIconLoader() = default;
ExtensionMenuIconLoader::~ExtensionMenuIconLoader() = default;

void ExtensionMenuIconLoader::LoadIcon(
    content::BrowserContext* context,
    const Extension* extension,
    const MenuItem::ExtensionKey& extension_key) {
  icon_manager_.LoadIcon(context, extension);
}

gfx::Image ExtensionMenuIconLoader::GetIcon(
    const MenuItem::ExtensionKey& extension_key) {
  return icon_manager_.GetIcon(extension_key.extension_id);
}

void ExtensionMenuIconLoader::RemoveIcon(
    const MenuItem::ExtensionKey& extension_key) {
  icon_manager_.RemoveIcon(extension_key.extension_id);
}

}  // namespace extensions
