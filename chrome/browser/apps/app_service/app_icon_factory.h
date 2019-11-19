// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class BrowserContext;
}

namespace apps {

// A bitwise-or of icon post-processing effects.
//
// It derives from a uint32_t because it needs to be the same size as the
// uint32_t IconKey.icon_effects field.
enum IconEffects : uint32_t {
  kNone = 0x00,

  // The icon effects are applied in numerical order, low to high. It is always
  // resize-and-then-badge and never badge-and-then-resize, which can matter if
  // the badge has a fixed size.
  kResizeAndPad = 0x01,  // Resize and Pad per Material Design style.
  kBadge = 0x02,         // Another (Android) app has the same name.
  kGray = 0x04,          // Disabled apps are grayed out.
  kRoundCorners = 0x08,  // Bookmark apps get round corners.
  kPaused = 0x10,        // Paused apps are badged to indicate they cannot be
                         // launched.
};

// Modifies |image_skia| to apply icon post-processing effects like badging and
// desaturation to gray.
void ApplyIconEffects(IconEffects icon_effects,
                      int size_hint_in_dip,
                      gfx::ImageSkia* image_skia);

// Loads an icon from an extension.
void LoadIconFromExtension(apps::mojom::IconCompression icon_compression,
                           int size_hint_in_dip,
                           content::BrowserContext* context,
                           const std::string& extension_id,
                           IconEffects icon_effects,
                           apps::mojom::Publisher::LoadIconCallback callback);

// Loads an icon from a FilePath. If that fails, it calls the fallback.
//
// The file named by |path| might be empty, not found or otherwise unreadable.
// If so, "fallback(callback)" is run. If the file is non-empty and readable,
// just "callback" is run, even if that file doesn't contain a valid image.
void LoadIconFromFileWithFallback(
    apps::mojom::IconCompression icon_compression,
    int size_hint_in_dip,
    const base::FilePath& path,
    IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
        fallback);

// Loads an icon from a compiled-into-the-binary resource, with a resource_id
// named IDR_XXX, for some value of XXX.
void LoadIconFromResource(apps::mojom::IconCompression icon_compression,
                          int size_hint_in_dip,
                          int resource_id,
                          bool is_placeholder_icon,
                          IconEffects icon_effects,
                          apps::mojom::Publisher::LoadIconCallback callback);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_
