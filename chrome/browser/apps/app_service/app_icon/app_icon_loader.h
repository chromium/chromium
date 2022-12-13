// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_LOADER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_LOADER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "extensions/common/constants.h"
#include "ui/gfx/image/image_skia.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ui/base/resource/resource_scale_factor.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace arc {
class IconDecodeRequest;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}

class Profile;

namespace web_app {
class WebAppIconManager;
}

namespace apps {

// This class is meant to:
// * Simplify loading icons, as things like effects and type are common
//   to all loading.
// * Allow the caller to halt the process by destructing the loader at any time,
// * Allow easy additions to the icon loader if necessary (like new effects or
// backups).
// Must be created & run from the UI thread.
class AppIconLoader : public base::RefCounted<AppIconLoader> {
 public:
  static const int kFaviconFallbackImagePx =
      extension_misc::EXTENSION_ICON_BITTY;

  AppIconLoader(IconType icon_type,
                int size_hint_in_dip,
                bool is_placeholder_icon,
                apps::IconEffects icon_effects,
                int fallback_icon_resource,
                LoadIconCallback callback);

  AppIconLoader(IconType icon_type,
                int size_hint_in_dip,
                bool is_placeholder_icon,
                apps::IconEffects icon_effects,
                int fallback_icon_resource,
                base::OnceCallback<void(LoadIconCallback)> fallback,
                LoadIconCallback callback);

  AppIconLoader(int size_hint_in_dip,
                base::OnceCallback<void(const gfx::ImageSkia& icon)> callback);

  explicit AppIconLoader(
      base::OnceCallback<void(const std::vector<gfx::ImageSkia>& icons)>
          callback);

  AppIconLoader(int size_hint_in_dip, LoadIconCallback callback);

  void ApplyIconEffects(IconEffects icon_effects, IconValuePtr iv);

  void ApplyBadges(IconEffects icon_effects, IconValuePtr iv);

  void LoadWebAppIcon(const std::string& web_app_id,
                      const GURL& launch_url,
                      web_app::WebAppIconManager& icon_manager,
                      Profile* profile);

  void LoadExtensionIcon(const extensions::Extension* extension,
                         content::BrowserContext* context);

  // The image file must be compressed using the default encoding.
  void LoadCompressedIconFromFile(const base::FilePath& path);
  void LoadIconFromCompressedData(const std::string& compressed_icon_data);

  void LoadIconFromResource(int icon_resource);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For ARC icons, converts an icon png data to an ImageSkia using
  // arc::IconDecodeRequest.
  void LoadArcIconPngData(const std::vector<uint8_t>& icon_png_data);

  // For ARC icons, composite the foreground image and the background image,
  // then apply the mask.
  void LoadCompositeImages(const std::vector<uint8_t>& foreground_data,
                           const std::vector<uint8_t>& background_data);

  // Loads icons for ARC activities.
  void LoadArcActivityIcons(
      const std::vector<arc::mojom::ActivityIconPtr>& icons);

  // Requests a compressed icon data with `scale_factor` for an web app
  // identified by `web_app_id`.
  void GetWebAppCompressedIconData(const std::string& web_app_id,
                                   ui::ResourceScaleFactor scale_factor,
                                   web_app::WebAppIconManager& icon_manager);

  // Requests a compressed icon data with `scale_factor` for a chrome app
  // identified by `extension`.
  void GetChromeAppCompressedIconData(const extensions::Extension* extension,
                                      content::BrowserContext* context,
                                      ui::ResourceScaleFactor scale_factor);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  friend class base::RefCounted<AppIconLoader>;

  ~AppIconLoader();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<arc::IconDecodeRequest> CreateArcIconDecodeRequest(
      base::OnceCallback<void(const gfx::ImageSkia& icon)> callback,
      const std::vector<uint8_t>& icon_png_data);

  void ApplyBackgroundAndMask(const gfx::ImageSkia& image);

  void CompositeImagesAndApplyMask(bool is_foreground,
                                   const gfx::ImageSkia& image);

  void OnArcActivityIconLoaded(gfx::ImageSkia* arc_activity_icon,
                               const gfx::ImageSkia& icon);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void MaybeApplyEffectsAndComplete(const gfx::ImageSkia image);

  void CompleteWithCompressed(bool is_maskable_icon, std::vector<uint8_t> data);

  void CompleteWithUncompressed(IconValuePtr iv);

  void CompleteWithIconValue(IconValuePtr iv);

  void OnReadWebAppIcon(std::map<int, SkBitmap> icon_bitmaps);

  void OnReadWebAppForCompressedIconData(bool is_maskable_icon,
                                         std::map<int, SkBitmap> icon_bitmaps);

  void OnReadChromeAppForCompressedIconData(gfx::ImageSkia image);

  void MaybeLoadFallbackOrCompleteEmpty();

  const IconType icon_type_ = IconType::kUnknown;

  const int size_hint_in_dip_ = 0;
  int icon_size_in_px_ = 0;
  // The scale factor the icon is intended for. See gfx::ImageSkiaRep::scale
  // comments.
  float icon_scale_ = 0.0f;
  // A scale factor to take as input for the IconType::kCompressed response. See
  // gfx::ImageSkia::GetRepresentation() comments.
  float icon_scale_for_compressed_response_ = 1.0f;

  const bool is_placeholder_icon_ = false;
  apps::IconEffects icon_effects_;

  // If |fallback_favicon_url_| is populated, then the favicon service is the
  // first fallback method attempted in MaybeLoadFallbackOrCompleteEmpty().
  // These members are only populated from LoadWebAppIcon or LoadExtensionIcon.
  GURL fallback_favicon_url_;
  raw_ptr<Profile> profile_ = nullptr;

  // If |fallback_icon_resource_| is not |kInvalidIconResource|, then it is the
  // second fallback method attempted in MaybeLoadFallbackOrCompleteEmpty()
  // (after the favicon service).
  int fallback_icon_resource_ = kInvalidIconResource;

  LoadIconCallback callback_;

  // A custom fallback operation to try.
  base::OnceCallback<void(LoadIconCallback)> fallback_callback_;

  base::CancelableTaskTracker cancelable_task_tracker_;

  // A sequenced task runner to create standard icons and not spamming the
  // thread pool.
  scoped_refptr<base::SequencedTaskRunner> standard_icon_task_runner_;

  gfx::ImageSkia foreground_image_;
  gfx::ImageSkia background_image_;
  bool foreground_is_set_ = false;
  bool background_is_set_ = false;
  base::OnceCallback<void(const gfx::ImageSkia& icon)> image_skia_callback_;

  std::vector<gfx::ImageSkia> arc_activity_icons_;
  size_t count_ = 0;
  base::OnceCallback<void(const std::vector<gfx::ImageSkia>& icon)>
      arc_activity_icons_callback_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<arc::IconDecodeRequest> arc_icon_decode_request_;
  std::unique_ptr<arc::IconDecodeRequest> arc_foreground_icon_decode_request_;
  std::unique_ptr<arc::IconDecodeRequest> arc_background_icon_decode_request_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_LOADER_H_
