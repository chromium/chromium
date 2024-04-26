// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/debug/stack_trace.h"
#include "base/debug/task_trace.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_loader.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/grit/app_icon_resources.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Copy from Android code, all four sides of the ARC foreground and background
// images are padded 25% of it's width and height.
float kAndroidAdaptiveIconPaddingPercentage = 1.0f / 8.0f;

bool IsConsistentPixelSize(const gfx::ImageSkiaRep& rep,
                           const gfx::ImageSkia& image_skia) {
  // The pixel size calculation method must be consistent with
  // ArcAppIconDescriptor::GetSizeInPixels.
  return rep.pixel_width() == roundf(image_skia.width() * rep.scale()) &&
         rep.pixel_height() == roundf(image_skia.height() * rep.scale());
}

// Return whether the image_reps in |image_skia| should be chopped for
// paddings. If the image_rep's pixel size is inconsistent with the scaled width
// and height, that means the image_rep has paddings and should be chopped to
// remove paddings. Otherwise, the image_rep doesn't have padding, and should
// not be chopped.
bool ShouldExtractSubset(const gfx::ImageSkia& image_skia) {
  DCHECK(!image_skia.image_reps().empty());
  for (const auto& rep : image_skia.image_reps()) {
    if (!IsConsistentPixelSize(rep, image_skia)) {
      return true;
    }
  }
  return false;
}

// Chop paddings for all four sides of |image_skia|, and resize image_rep to the
// appropriate size.
gfx::ImageSkia ExtractSubsetForArcImage(const gfx::ImageSkia& image_skia) {
  gfx::ImageSkia subset_image;
  for (const auto& rep : image_skia.image_reps()) {
    if (IsConsistentPixelSize(rep, image_skia)) {
      subset_image.AddRepresentation(rep);
      continue;
    }

    int padding_width =
        rep.pixel_width() * kAndroidAdaptiveIconPaddingPercentage;
    int padding_height =
        rep.pixel_height() * kAndroidAdaptiveIconPaddingPercentage;

    // Chop paddings for all four sides of |image_skia|.
    SkBitmap dst;
    bool success = rep.GetBitmap().extractSubset(
        &dst,
        RectToSkIRect(gfx::Rect(padding_width, padding_height,
                                rep.pixel_width() - 2 * padding_width,
                                rep.pixel_height() - 2 * padding_height)));
    DCHECK(success);

    // Resize |rep| to roundf(size_hint_in_dip * rep.scale()), to keep
    // consistency with ArcAppIconDescriptor::GetSizeInPixels.
    const SkBitmap resized = skia::ImageOperations::Resize(
        dst, skia::ImageOperations::RESIZE_LANCZOS3,
        roundf(image_skia.width() * rep.scale()),
        roundf(image_skia.height() * rep.scale()));
    subset_image.AddRepresentation(gfx::ImageSkiaRep(resized, rep.scale()));
  }
  return subset_image;
}
#endif

using SizeToImageSkiaRep = std::map<int, gfx::ImageSkiaRep>;
using ScaleToImageSkiaReps = std::map<float, SizeToImageSkiaRep>;
using MaskImageSkiaReps = std::pair<SkBitmap, ScaleToImageSkiaReps>;

MaskImageSkiaReps& GetMaskResourceIconCache() {
  static base::NoDestructor<MaskImageSkiaReps> mask_cache;
  return *mask_cache;
}

const SkBitmap& GetMaskBitmap() {
  auto& mask_cache = GetMaskResourceIconCache();
  if (mask_cache.first.empty()) {
    // We haven't yet loaded the mask image from resources. Do so and store it
    // in the cache.
    mask_cache.first = *ui::ResourceBundle::GetSharedInstance()
                            .GetImageNamed(IDR_ICON_MASK)
                            .ToSkBitmap();
  }
  DCHECK(!mask_cache.first.empty());
  return mask_cache.first;
}

// Returns the mask image corresponding to the given image |scale| and edge
// pixel |size|. The mask must precisely match the properties of the image it
// will be composited onto.
const gfx::ImageSkiaRep& GetMaskAsImageSkiaRep(float scale,
                                               int size_hint_in_dip) {
  auto& mask_cache = GetMaskResourceIconCache();
  const auto& scale_iter = mask_cache.second.find(scale);
  if (scale_iter != mask_cache.second.end()) {
    const auto& size_iter = scale_iter->second.find(size_hint_in_dip);
    if (size_iter != scale_iter->second.end()) {
      return size_iter->second;
    }
  }

  auto& image_rep = mask_cache.second[scale][size_hint_in_dip];
  image_rep = gfx::ImageSkiaRep(
      skia::ImageOperations::Resize(GetMaskBitmap(),
                                    skia::ImageOperations::RESIZE_LANCZOS3,
                                    size_hint_in_dip, size_hint_in_dip),
      scale);
  return image_rep;
}

// Calls |callback| with the compressed icon |data|.
void CompleteIconWithCompressed(apps::LoadIconCallback callback,
                                std::vector<uint8_t> data) {
  if (data.empty()) {
    std::move(callback).Run(std::make_unique<apps::IconValue>());
    return;
  }
  auto iv = std::make_unique<apps::IconValue>();
  iv->icon_type = apps::IconType::kCompressed;
  iv->compressed = std::move(data);
  iv->is_placeholder_icon = false;
  std::move(callback).Run(std::move(iv));
}

}  // namespace

namespace apps {

std::map<std::pair<int, int>, gfx::ImageSkia>& GetResourceIconCache() {
  static base::NoDestructor<std::map<std::pair<int, int>, gfx::ImageSkia>>
      cache;
  return *cache;
}

gfx::ImageSkia CreateResizedResourceImage(int icon_resource,
                                          int32_t size_in_dip) {
  TRACE_EVENT0("ui", "apps::CreateResizedResourceImage");
  // Get the ImageSkia for the resource `icon_resource`. The
  // ui::ResourceBundle shared instance already caches ImageSkia's, but caches
  // the unscaled versions. The `cache` here caches scaled versions, keyed by
  // the pair (`icon_resource`, `size_in_dip`).
  std::map<std::pair<int, int>, gfx::ImageSkia>& cache = GetResourceIconCache();
  const auto cache_key = std::make_pair(icon_resource, size_in_dip);
  const auto cache_iter = cache.find(cache_key);
  if (cache_iter != cache.end()) {
    return cache_iter->second;
  }

  gfx::ImageSkia* default_image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_resource);
  CHECK(default_image);
  gfx::ImageSkia image_skia = gfx::ImageSkiaOperations::CreateResizedImage(
      *default_image, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(size_in_dip, size_in_dip));
  cache.insert(std::make_pair(cache_key, image_skia));
  return image_skia;
}

apps::ScaleToSize GetScaleToSize(const gfx::ImageSkia& image_skia) {
  TRACE_EVENT0("ui", "apps::GetScaleToSize");
  apps::ScaleToSize scale_to_size;
  if (image_skia.image_reps().empty()) {
    scale_to_size[1.0f] = image_skia.size().width();
  } else {
    for (const auto& rep : image_skia.image_reps()) {
      scale_to_size[rep.scale()] = rep.pixel_width();
    }
  }
  return scale_to_size;
}

void CompressedDataToSkBitmap(
    base::span<const uint8_t> compressed_data,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  TRACE_EVENT0("ui", "apps::CompressedDataToSkBitmap");
  if (compressed_data.empty()) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  data_decoder::DecodeImage(
      &GetIconDataDecoder(), compressed_data,
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(), std::move(callback));
}

void CompressedDataToImageSkia(
    base::span<const uint8_t> compressed_data,
    float icon_scale,
    base::OnceCallback<void(gfx::ImageSkia)> callback) {
  CompressedDataToSkBitmap(
      compressed_data,
      base::BindOnce(
          [](base::OnceCallback<void(gfx::ImageSkia)> inner_callback,
             float icon_scale, const SkBitmap& bitmap) {
            std::move(inner_callback)
                .Run(SkBitmapToImageSkia(bitmap, icon_scale));
          },
          std::move(callback), icon_scale));
}

base::OnceCallback<void(std::vector<uint8_t> compressed_data)>
CompressedDataToImageSkiaCallback(
    base::OnceCallback<void(gfx::ImageSkia)> callback,
    float icon_scale) {
  return base::BindOnce(
      [](base::OnceCallback<void(gfx::ImageSkia)> inner_callback,
         float icon_scale, std::vector<uint8_t> compressed_data) {
        TRACE_EVENT0("ui", "apps::CompressedDataToImageSkiaCallback::Run");
        CompressedDataToImageSkia(compressed_data, icon_scale,
                                  std::move(inner_callback));
      },
      std::move(callback), icon_scale);
}

gfx::ImageSkia SkBitmapToImageSkia(const SkBitmap& bitmap, float icon_scale) {
  return gfx::ImageSkia::CreateFromBitmap(bitmap, icon_scale);
}

std::vector<uint8_t> EncodeImageToPngBytes(const gfx::ImageSkia image,
                                           float rep_icon_scale) {
  TRACE_EVENT0("ui", "apps::EncodeImageToPngBytes");
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<uint8_t> image_data;

  const gfx::ImageSkiaRep& image_skia_rep =
      image.GetRepresentation(rep_icon_scale);
  if (image_skia_rep.scale() != rep_icon_scale) {
    return image_data;
  }

  const SkBitmap& bitmap = image_skia_rep.GetBitmap();
  if (bitmap.drawsNothing()) {
    return image_data;
  }

  base::AssertLongCPUWorkAllowed();
  constexpr bool discard_transparency = false;
  bool success = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency,
                                                   &image_data);
  if (!success) {
    return std::vector<uint8_t>();
  }
  return image_data;
}

gfx::ImageSkia LoadMaskImage(const ScaleToSize& scale_to_size) {
  TRACE_EVENT0("ui", "apps::LoadMaskImage");
  gfx::ImageSkia mask_image;
  for (const auto& it : scale_to_size) {
    float scale = it.first;
    int size_hint_in_dip = it.second;
    mask_image.AddRepresentation(
        GetMaskAsImageSkiaRep(scale, size_hint_in_dip));
  }

  return mask_image;
}

gfx::ImageSkia ApplyBackgroundAndMask(const gfx::ImageSkia& image) {
  TRACE_EVENT0("ui", "apps::ApplyBackgroundAndMask");
  if (image.isNull()) {
    return gfx::ImageSkia();
  }
  return gfx::ImageSkiaOperations::CreateButtonBackground(
      SK_ColorWHITE, image, LoadMaskImage(GetScaleToSize(image)));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
gfx::ImageSkia CompositeImagesAndApplyMask(
    const gfx::ImageSkia& foreground_image,
    const gfx::ImageSkia& background_image) {
  TRACE_EVENT0("ui", "apps::CompositeImagesAndApplyMask");
  bool should_extract_subset_foreground = ShouldExtractSubset(foreground_image);
  bool should_extract_subset_background = ShouldExtractSubset(background_image);

  if (!should_extract_subset_foreground && !should_extract_subset_background) {
    return gfx::ImageSkiaOperations::CreateMaskedImage(
        gfx::ImageSkiaOperations::CreateSuperimposedImage(background_image,
                                                          foreground_image),
        LoadMaskImage(GetScaleToSize(foreground_image)));
  }

  // If the foreground or background image has padding, chop the padding of the
  // four sides, and resize the image_reps for different scales.
  gfx::ImageSkia foreground = should_extract_subset_foreground
                                  ? ExtractSubsetForArcImage(foreground_image)
                                  : foreground_image;
  gfx::ImageSkia background = should_extract_subset_background
                                  ? ExtractSubsetForArcImage(background_image)
                                  : background_image;

  return gfx::ImageSkiaOperations::CreateMaskedImage(
      gfx::ImageSkiaOperations::CreateSuperimposedImage(background, foreground),
      LoadMaskImage(GetScaleToSize(foreground)));
}

void ArcRawIconPngDataToImageSkia(
    arc::mojom::RawIconPngDataPtr icon,
    int size_hint_in_dip,
    base::OnceCallback<void(const gfx::ImageSkia& icon)> callback) {
  TRACE_EVENT0("ui", "apps::ArcRawIconPngDataToImageSkia");
  if (!icon) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  // For non-adaptive icons, add the white color background, and apply the mask.
  if (!icon->is_adaptive_icon) {
    if (!icon->icon_png_data.has_value()) {
      std::move(callback).Run(gfx::ImageSkia());
      return;
    }

    scoped_refptr<AppIconLoader> icon_loader =
        base::MakeRefCounted<AppIconLoader>(size_hint_in_dip,
                                            std::move(callback));
    icon_loader->LoadArcIconPngData(icon->icon_png_data.value());
    return;
  }

  if (!icon->foreground_icon_png_data.has_value() ||
      icon->foreground_icon_png_data.value().empty() ||
      !icon->background_icon_png_data.has_value() ||
      icon->background_icon_png_data.value().empty()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  // For adaptive icons, composite the background and the foreground images
  // together, then applying the mask
  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(size_hint_in_dip,
                                          std::move(callback));
  icon_loader->LoadCompositeImages(
      std::move(icon->foreground_icon_png_data.value()),
      std::move(icon->background_icon_png_data.value()));
}

void ArcActivityIconsToImageSkias(
    const std::vector<arc::mojom::ActivityIconPtr>& icons,
    base::OnceCallback<void(const std::vector<gfx::ImageSkia>& icons)>
        callback) {
  TRACE_EVENT0("ui", "apps::ArcActivityIconsToImageSkias");
  if (icons.empty()) {
    std::move(callback).Run(std::vector<gfx::ImageSkia>{});
    return;
  }

  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(std::move(callback));
  icon_loader->LoadArcActivityIcons(icons);
}

gfx::ImageSkia ConvertSquareBitmapsToImageSkia(
    const std::map<web_app::SquareSizePx, SkBitmap>& icon_bitmaps,
    IconEffects icon_effects,
    int size_hint_in_dip) {
  TRACE_EVENT0("ui", "apps::ConvertSquareBitmapsToImageSkia");
  auto image_skia =
      ConvertIconBitmapsToImageSkia(icon_bitmaps, size_hint_in_dip);

  if (image_skia.isNull()) {
    return gfx::ImageSkia{};
  }

  if ((icon_effects & IconEffects::kCrOsStandardMask) &&
      (icon_effects & IconEffects::kCrOsStandardBackground)) {
    image_skia = apps::ApplyBackgroundAndMask(image_skia);
  }
  return image_skia;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

gfx::ImageSkia ConvertIconBitmapsToImageSkia(
    const std::map<web_app::SquareSizePx, SkBitmap>& icon_bitmaps,
    int size_hint_in_dip) {
  TRACE_EVENT0("ui", "apps::ConvertIconBitmapsToImageSkia");
  if (icon_bitmaps.empty()) {
    return gfx::ImageSkia{};
  }

  gfx::ImageSkia image_skia;
  auto it = icon_bitmaps.begin();

  for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    float icon_scale = ui::GetScaleForResourceScaleFactor(scale_factor);

    web_app::SquareSizePx icon_size_in_px =
        gfx::ScaleToFlooredSize(gfx::Size(size_hint_in_dip, size_hint_in_dip),
                                icon_scale)
            .width();

    while (it != icon_bitmaps.end() && it->first < icon_size_in_px) {
      ++it;
    }

    if (it == icon_bitmaps.end() || it->second.empty()) {
      continue;
    }

    SkBitmap bitmap = it->second;
    // Resize |bitmap| to match |icon_scale|.
    //
    // TODO(crbug.com/40755741): All conversions in app_icon_factory.cc must
    // perform CPU-heavy operations off the Browser UI thread.
    if (bitmap.width() != icon_size_in_px) {
      bitmap = skia::ImageOperations::Resize(
          bitmap, skia::ImageOperations::RESIZE_LANCZOS3, icon_size_in_px,
          icon_size_in_px);
    }

    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, icon_scale));
  }

  if (image_skia.isNull()) {
    return gfx::ImageSkia{};
  }

  image_skia.EnsureRepsForSupportedScales();

  return image_skia;
}

void ApplyIconEffects(Profile* profile,
                      const std::optional<std::string>& app_id,
                      IconEffects icon_effects,
                      int size_hint_in_dip,
                      IconValuePtr iv,
                      LoadIconCallback callback) {
  TRACE_EVENT0("ui", "apps::ApplyIconEffects");
  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(profile, size_hint_in_dip,
                                          std::move(callback));
  icon_loader->ApplyIconEffects(icon_effects, app_id, std::move(iv));
}

void ConvertUncompressedIconToCompressedIconWithScale(float rep_icon_scale,
                                                      LoadIconCallback callback,
                                                      IconValuePtr iv) {
  TRACE_EVENT0("ui", "apps::ConvertUncompressedIconToCompressedIconWithScale");
  if (!iv) {
    std::move(callback).Run(std::make_unique<apps::IconValue>());
    return;
  }

  iv->uncompressed.MakeThreadSafe();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&apps::EncodeImageToPngBytes, iv->uncompressed,
                     /*rep_icon_scale=*/rep_icon_scale),
      base::BindOnce(&CompleteIconWithCompressed, std::move(callback)));
}

void ConvertUncompressedIconToCompressedIcon(IconValuePtr iv,
                                             LoadIconCallback callback) {
  ConvertUncompressedIconToCompressedIconWithScale(
      /*rep_icon_scale=*/1.0f, std::move(callback), std::move(iv));
}

void LoadIconFromExtension(IconType icon_type,
                           int size_hint_in_dip,
                           Profile* profile,
                           const std::string& extension_id,
                           IconEffects icon_effects,
                           LoadIconCallback callback) {
  TRACE_EVENT0("ui", "apps::LoadIconFromExtension");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  constexpr bool is_placeholder_icon = false;
  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          profile, /*app_id=*/std::nullopt, icon_type, size_hint_in_dip,
          is_placeholder_icon, icon_effects, IDR_APP_DEFAULT_ICON,
          std::move(callback));
  icon_loader->LoadExtensionIcon(
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          extension_id));
}

void LoadIconFromWebApp(Profile* profile,
                        IconType icon_type,
                        int size_hint_in_dip,
                        const std::string& web_app_id,
                        IconEffects icon_effects,
                        LoadIconCallback callback) {
  TRACE_EVENT0("ui", "apps::LoadIconFromWebApp");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(profile);
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);

  CHECK(web_app_provider);
  constexpr bool is_placeholder_icon = false;
  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          profile, /*app_id=*/std::nullopt, icon_type, size_hint_in_dip,
          is_placeholder_icon, icon_effects, IDR_APP_DEFAULT_ICON,
          std::move(callback));
  icon_loader->LoadWebAppIcon(
      web_app_id,
      web_app_provider->registrar_unsafe().GetAppStartUrl(web_app_id),
      web_app_provider->icon_manager());
}

#if BUILDFLAG(IS_CHROMEOS)
void GetWebAppCompressedIconData(Profile* profile,
                                 const std::string& web_app_id,
                                 int size_in_dip,
                                 ui::ResourceScaleFactor scale_factor,
                                 LoadIconCallback callback) {
  TRACE_EVENT0("ui", "apps::GetWebAppCompressedIconData");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(profile);
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);

  DCHECK(web_app_provider);
  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          profile, /*app_id=*/std::nullopt, IconType::kCompressed, size_in_dip,
          /*is_placeholder_icon=*/false, IconEffects::kNone,
          kInvalidIconResource, std::move(callback));
  icon_loader->GetWebAppCompressedIconData(web_app_id, scale_factor,
                                           web_app_provider->icon_manager());
}

void GetChromeAppCompressedIconData(Profile* profile,
                                    const std::string& extension_id,
                                    int size_in_dip,
                                    ui::ResourceScaleFactor scale_factor,
                                    LoadIconCallback callback) {
  TRACE_EVENT0("ui", "apps::GetChromeAppCompressedIconData");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          profile, /*app_id=*/std::nullopt, IconType::kCompressed, size_in_dip,
          /*is_placeholder_icon=*/false, IconEffects::kNone,
          IDR_APP_DEFAULT_ICON, std::move(callback));
  icon_loader->GetChromeAppCompressedIconData(
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          extension_id),
      scale_factor);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void GetArcAppCompressedIconData(Profile* profile,
                                 const std::string& app_id,
                                 int size_in_dip,
                                 ui::ResourceScaleFactor scale_factor,
                                 LoadIconCallback callback) {
  TRACE_EVENT0("ui", "apps::GetArcAppCompressedIconData");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(profile);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  if (!prefs) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return;
  }

  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          profile, app_id, IconType::kCompressed, size_in_dip,
          /*is_placeholder_icon=*/false, IconEffects::kNone,
          kInvalidIconResource, std::move(callback));
  icon_loader->GetArcAppCompressedIconData(app_id, prefs, scale_factor);
}

void GetGuestOSAppCompressedIconData(Profile* profile,
                                     const std::string& app_id,
                                     int size_in_dip,
                                     ui::ResourceScaleFactor scale_factor,
                                     LoadIconCallback callback) {
  TRACE_EVENT0("ui", "apps::GetGuestOSAppCompressedIconData");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(profile);

  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          profile, app_id, IconType::kCompressed, size_in_dip,
          /*is_placeholder_icon=*/false, IconEffects::kNone,
          kInvalidIconResource, std::move(callback));
  icon_loader->GetGuestOSAppCompressedIconData(app_id, scale_factor);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void LoadIconFromFileWithFallback(
    IconType icon_type,
    int size_hint_in_dip,
    const base::FilePath& path,
    IconEffects icon_effects,
    LoadIconCallback callback,
    base::OnceCallback<void(LoadIconCallback)> fallback) {
  TRACE_EVENT0("ui", "apps::LoadIconFromFileWithFallback");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  constexpr bool is_placeholder_icon = false;

  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          /*profile=*/nullptr, /*app_id=*/std::nullopt, icon_type,
          size_hint_in_dip, is_placeholder_icon, icon_effects,
          kInvalidIconResource, std::move(fallback), std::move(callback));
  icon_loader->LoadCompressedIconFromFile(path);
}

void LoadIconFromCompressedData(IconType icon_type,
                                int size_hint_in_dip,
                                IconEffects icon_effects,
                                const std::string& compressed_icon_data,
                                LoadIconCallback callback) {
  TRACE_EVENT0("ui", "apps::LoadIconFromCompressedData");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  constexpr bool is_placeholder_icon = false;

  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          /*profile=*/nullptr, /*app_id=*/std::nullopt, icon_type,
          size_hint_in_dip, is_placeholder_icon, icon_effects,
          kInvalidIconResource, std::move(callback));
  icon_loader->LoadIconFromCompressedData(compressed_icon_data);
}

void LoadIconFromResource(Profile* profile,
                          std::optional<std::string> app_id,
                          IconType icon_type,
                          int size_hint_in_dip,
                          int resource_id,
                          bool is_placeholder_icon,
                          IconEffects icon_effects,
                          LoadIconCallback callback) {
  TRACE_EVENT0("ui", "apps::LoadIconFromResource");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // There is no fallback icon for a resource.
  constexpr int fallback_icon_resource = 0;

  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          profile, app_id, icon_type, size_hint_in_dip, is_placeholder_icon,
          icon_effects, fallback_icon_resource, std::move(callback));

  icon_loader->LoadIconFromResource(resource_id);
}
}  // namespace apps
