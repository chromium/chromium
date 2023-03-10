// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/extension_icon_loader.h"

#include "base/functional/bind.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

ExtensionIconLoader::ExtensionIconLoader() = default;
ExtensionIconLoader::~ExtensionIconLoader() = default;

void ExtensionIconLoader::Load(const extensions::Extension& extension,
                               content::BrowserContext* context,
                               IconLoadCallback icon_load_callback) {
  DCHECK(context);

  icon_load_callback_ = std::move(icon_load_callback);
  auto load_image_callback =
      base::BindOnce(&ExtensionIconLoader::OnIconLoaded, factory_.GetWeakPtr(),
                     extension.is_app());
  extensions::ImageLoader::Get(context)->LoadImageAtEveryScaleFactorAsync(
      &extension,
      gfx::Size(extension_misc::EXTENSION_ICON_LARGE,
                extension_misc::EXTENSION_ICON_LARGE),
      std::move(load_image_callback));
}

void ExtensionIconLoader::OnIconLoaded(bool is_app, const gfx::Image& image) {
  if (!image.IsEmpty()) {
    std::move(icon_load_callback_).Run(*image.ToImageSkia());
  } else {
    std::move(icon_load_callback_)
        .Run(is_app ? extensions::util::GetDefaultAppIcon()
                    : extensions::util::GetDefaultExtensionIcon());
  }
}
}  // namespace extensions
