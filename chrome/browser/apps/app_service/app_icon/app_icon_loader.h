// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_LOADER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_LOADER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/profiles/profile_observer.h"
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

namespace extensions {
class Extension;
}

class ArcAppListPrefs;
class Profile;

namespace web_app {
class WebAppIconManager;
}

class SkBitmap;

namespace apps {

class SvgIconTranscoder;

// This struct is used to record the icon paths for the adaptive icon.
struct AdaptiveIconPaths {
  // Returns true when all paths are empty. Otherwise, returns false.
  bool IsEmpty();

  // The raw icon path for the non-adaptive icon.
  base::FilePath icon_path;
  // The foreground icon path for the adaptive icon.
  base::FilePath foreground_icon_path;
  // The background icon path for the adaptive icon.
  base::FilePath background_icon_path;
};

// This class is meant to:
// * Simplify loading icons, as things like effects and type are common
//   to all loading.
// * Allow the caller to halt the process by destructing the loader at any time,
// * Allow easy additions to the icon loader if necessary (like new effects or
// backups).
// Must be created & run from the UI thread.
class AppIconLoader : public base::RefCounted<AppIconLoader>,
                      public ProfileObserver {
 public:
  static const int kFaviconFallbackImagePx =
      extension_misc::EXTENSION_ICON_BITTY;

  AppIconLoader(Profile* profile,
                std::optional<std::string> app_id,
                IconType icon_type,
                int size_hint_in_dip,
                bool is_placeholder_icon,
                apps::IconEffects icon_effects,
                int fallback_icon_resource,
                LoadIconCallback callback);

  AppIconLoader(Profile* profile,
                std::optional<std::string> app_id,
                IconType icon_type,
                int size_hint_in_dip,
                bool is_placeholder_icon,
                apps::IconEffects icon_effects,
                int fallback_icon_resource,
                base::OnceCallback<void(LoadIconCallback)> fallback,
                LoadIconCallback callback);

  AppIconLoader(Profile* profile,
                int size_hint_in_dip,
                LoadIconCallback callback);

  AppIconLoader(int size_hint_in_dip,
                base::OnceCallback<void(const gfx::ImageSkia& icon)> callback);

  explicit AppIconLoader(
      base::OnceCallback<void(const std::vector<gfx::ImageSkia>& icons)>
          callback);

  AppIconLoader(int size_hint_in_dip, LoadIconCallback callback);

  void ApplyIconEffects(IconEffects icon_effects,
                        const std::optional<std::string>& app_id,
                        IconValuePtr iv);

  void ApplyBadges(IconEffects icon_effects,
                   const std::optional<std::string>& app_id,
                   IconValuePtr iv);

  void LoadWebAppIcon(const std::string& web_app_id,
                      const GURL& launch_url,
                      web_app::WebAppIconManager& icon_manager);

  void LoadExtensionIcon(const extensions::Extension* extension);

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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
  // Requests a compressed icon data with `scale_factor` for an web app
  // identified by `web_app_id`.
  void GetWebAppCompressedIconData(const std::string& web_app_id,
                                   ui::ResourceScaleFactor scale_factor,
                                   web_app::WebAppIconManager& icon_manager);

  // Requests a compressed icon data with `scale_factor` for a chrome app
  // identified by `extension`.
  void GetChromeAppCompressedIconData(const extensions::Extension* extension,
                                      ui::ResourceScaleFactor scale_factor);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Requests a compressed icon data with `scale_factor` for an ARC app
  // identified by `app_id`.
  void GetArcAppCompressedIconData(const std::string& app_id,
                                   ArcAppListPrefs* arc_prefs,
                                   ui::ResourceScaleFactor scale_factor);

  // Requests a compressed icon data with `scale_factor` for a Guest OS app
  // identified by `app_id`.
  void GetGuestOSAppCompressedIconData(const std::string& app_id,
                                       ui::ResourceScaleFactor scale_factor);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  friend class base::RefCounted<AppIconLoader>;

  ~AppIconLoader() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnGetArcAppCompressedIconData(AdaptiveIconPaths app_icon_paths,
                                     arc::mojom::RawIconPngDataPtr icon);

  void OnGetGuestOSAppCompressedIconData(base::FilePath png_path,
                                         base::FilePath svg_path,
                                         std::string icon_data);

  void TranscodeIconFromSvg(base::FilePath svg_path, base::FilePath png_path);

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

  void OnGetCompressedIconDataWithSkBitmap(bool is_maskable_icon,
                                           const SkBitmap& bitmap);

  void OnReadChromeAppForCompressedIconData(gfx::ImageSkia image);

  void MaybeLoadFallbackOrCompleteEmpty();

  // ProfileObserver overrides.
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // If non-null, points to the profile necessary to support the icon loading.
  // Not used by all codepaths, so this isn't mandatory in the constructor,
  // and could be reset to null if the profile is torn down whilst async icon
  // loading is in-flight.
  raw_ptr<Profile> profile_ = nullptr;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  std::optional<std::string> app_id_ = std::nullopt;

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
  bool is_maskable_icon_ = false;

  // If |fallback_favicon_url_| is populated, then the favicon service is the
  // first fallback method attempted in MaybeLoadFallbackOrCompleteEmpty().
  GURL fallback_favicon_url_;

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
  std::unique_ptr<SvgIconTranscoder> svg_icon_transcoder_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_LOADER_H_
