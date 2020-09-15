// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_CHROMEOS)
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#endif  // OS_CHROMEOS

namespace content {
class BrowserContext;
}

namespace apps {

using ScaleToSize = std::map<float, int>;

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
  kChromeBadge = 0x02,   // Another (Android) app has the same name.
  kBlocked = 0x04,       // Disabled apps are grayed out and badged.
  kRoundCorners = 0x08,  // Bookmark apps get round corners.
  kPaused = 0x10,  // Paused apps are grayed out and badged to indicate they
                   // cannot be launched.
  kPendingLocalLaunch = 0x20,  // Apps that are installed through sync, but
                               // have not been launched locally yet. They
                               // should appear gray until they are launched.
  kCrOsStandardBackground =
      0x40,                   // Add the white background to the standard icon.
  kCrOsStandardMask = 0x80,   // Apply the mask to the standard icon.
  kCrOsStandardIcon = 0x100,  // Add the white background, maybe shrink the
                              // icon, and apply the mask to the standard icon
                              // This effect combines kCrOsStandardBackground
                              // and kCrOsStandardMask together.
};

inline IconEffects operator|(IconEffects a, IconEffects b) {
  return static_cast<IconEffects>(static_cast<uint32_t>(a) |
                                  static_cast<uint32_t>(b));
}

inline IconEffects operator|=(IconEffects& a, IconEffects b) {
  a = a | b;
  return a;
}

inline IconEffects operator&(IconEffects a, uint32_t b) {
  return static_cast<IconEffects>(static_cast<uint32_t>(a) &
                                  static_cast<uint32_t>(b));
}

inline IconEffects operator&=(IconEffects& a, uint32_t b) {
  a = a & b;
  return a;
}

// Returns a callback that converts compressed data to an ImageSkia.
base::OnceCallback<void(std::vector<uint8_t> compressed_data)>
CompressedDataToImageSkiaCallback(
    base::OnceCallback<void(gfx::ImageSkia)> callback,
    float icon_scale);

// Encodes a single SkBitmap representation from the given ImageSkia to the
// compressed PNG data. |rep_icon_scale| argument denotes, which ImageSkiaRep to
// take as input. See ImageSkia::GetRepresentation() comments. Returns the
// encoded PNG data. This function should not be called on the UI thread.
std::vector<uint8_t> EncodeImageToPngBytes(const gfx::ImageSkia image,
                                           float rep_icon_scale);

#if defined(OS_CHROMEOS)
gfx::ImageSkia LoadMaskImage(const ScaleToSize& scale_to_size);

gfx::ImageSkia ApplyBackgroundAndMask(const gfx::ImageSkia& image);

gfx::ImageSkia CompositeImagesAndApplyMask(
    const gfx::ImageSkia& foreground_image,
    const gfx::ImageSkia& background_image);

void ArcRawIconPngDataToImageSkia(
    arc::mojom::RawIconPngDataPtr icon,
    int size_hint_in_dip,
    base::OnceCallback<void(const gfx::ImageSkia& icon)> callback);

void ArcActivityIconsToImageSkias(
    const std::vector<arc::mojom::ActivityIconPtr>& icons,
    base::OnceCallback<void(const std::vector<gfx::ImageSkia>& icons)>
        callback);
#endif  // OS_CHROMEOS

// Modifies |image_skia| to apply icon post-processing effects like badging and
// desaturation to gray.
void ApplyIconEffects(IconEffects icon_effects,
                      int size_hint_in_dip,
                      gfx::ImageSkia* image_skia);

// Loads an icon from an extension.
void LoadIconFromExtension(apps::mojom::IconType icon_type,
                           int size_hint_in_dip,
                           content::BrowserContext* context,
                           const std::string& extension_id,
                           IconEffects icon_effects,
                           apps::mojom::Publisher::LoadIconCallback callback);

// Loads an icon from a web app.
void LoadIconFromWebApp(content::BrowserContext* context,
                        apps::mojom::IconType icon_type,
                        int size_hint_in_dip,
                        const std::string& web_app_id,
                        IconEffects icon_effects,
                        apps::mojom::Publisher::LoadIconCallback callback);

// Loads an icon from a FilePath. If that fails, it calls the fallback.
//
// The file named by |path| might be empty, not found or otherwise unreadable.
// If so, "fallback(callback)" is run. If the file is non-empty and readable,
// just "callback" is run, even if that file doesn't contain a valid image.
//
// |fallback| should run its callback argument once complete, even on a
// failure. A failure should be indicated by passing nullptr, in which case the
// pipeline will use a generic fallback icon.
void LoadIconFromFileWithFallback(
    apps::mojom::IconType icon_type,
    int size_hint_in_dip,
    const base::FilePath& path,
    IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
        fallback);

// Creates an icon with the specified effects from |compressed_icon_data|.
void LoadIconFromCompressedData(
    apps::mojom::IconType icon_type,
    int size_hint_in_dip,
    IconEffects icon_effects,
    const std::string& compressed_icon_data,
    apps::mojom::Publisher::LoadIconCallback callback);

// Loads an icon from a compiled-into-the-binary resource, with a resource_id
// named IDR_XXX, for some value of XXX.
void LoadIconFromResource(apps::mojom::IconType icon_type,
                          int size_hint_in_dip,
                          int resource_id,
                          bool is_placeholder_icon,
                          IconEffects icon_effects,
                          apps::mojom::Publisher::LoadIconCallback callback);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_
