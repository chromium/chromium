// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/ash/arc/icon_decode_request.h"
#include "chrome/browser/ui/app_list/md_icon_normalizer.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/skia_util.h"
#endif

namespace {

static const int kInvalidIconResource = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Copy from Android code, all four sides of the ARC foreground and background
// images are padded 25% of it's width and height.
float kAndroidAdaptiveIconPaddingPercentage = 1.0f / 8.0f;

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

apps::ScaleToSize GetScaleToSize(const gfx::ImageSkia& image_skia) {
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

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::map<std::pair<int, int>, gfx::ImageSkia>& GetResourceIconCache() {
  static base::NoDestructor<std::map<std::pair<int, int>, gfx::ImageSkia>>
      cache;
  return *cache;
}

std::vector<uint8_t> ReadFileAsCompressedData(const base::FilePath path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string data;
  base::ReadFileToString(path, &data);
  return std::vector<uint8_t>(data.begin(), data.end());
}

std::vector<uint8_t> CompressedDataFromResource(
    extensions::ExtensionResource resource) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const base::FilePath& path = resource.GetFilePath();
  if (path.empty()) {
    return std::vector<uint8_t>();
  }
  return ReadFileAsCompressedData(path);
}

SkBitmap DecompressToSkBitmap(const unsigned char* data, size_t size) {
  base::AssertLongCPUWorkAllowed();
  SkBitmap decoded;
  bool success = gfx::PNGCodec::Decode(data, size, &decoded);
  DCHECK(success);
  return decoded;
}

gfx::ImageSkia SkBitmapToImageSkia(SkBitmap bitmap, float icon_scale) {
  return gfx::ImageSkia::CreateFromBitmap(bitmap, icon_scale);
}

// Returns a callback that converts a gfx::Image to an ImageSkia.
base::OnceCallback<void(const gfx::Image&)> ImageToImageSkia(
    base::OnceCallback<void(gfx::ImageSkia)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(gfx::ImageSkia)> callback,
         const gfx::Image& image) {
        std::move(callback).Run(image.AsImageSkia());
      },
      std::move(callback));
}

base::OnceCallback<void(const favicon_base::FaviconRawBitmapResult&)>
FaviconResultToImageSkia(base::OnceCallback<void(gfx::ImageSkia)> callback,
                         float icon_scale) {
  return base::BindOnce(
      [](base::OnceCallback<void(gfx::ImageSkia)> callback, float icon_scale,
         const favicon_base::FaviconRawBitmapResult& result) {
        if (!result.is_valid()) {
          std::move(callback).Run(gfx::ImageSkia());
          return;
        }
        // It would be nice to not do a memory copy here, but
        // DecodeImageIsolated requires a std::vector, and RefCountedMemory
        // doesn't supply that.
        std::move(apps::CompressedDataToImageSkiaCallback(std::move(callback),
                                                          icon_scale))
            .Run(std::vector<uint8_t>(
                result.bitmap_data->front(),
                result.bitmap_data->front() + result.bitmap_data->size()));
      },
      std::move(callback), icon_scale);
}

// Loads the compressed data of an icon at the requested size (or larger) for
// the given extension.
void LoadCompressedDataFromExtension(
    const extensions::Extension* extension,
    int size_hint_in_px,
    base::OnceCallback<void(std::vector<uint8_t>)> compressed_data_callback) {
  // Load some component extensions' icons from statically compiled
  // resources (built into the Chrome binary), and other extensions'
  // icons (whether component extensions or otherwise) from files on
  // disk.
  extensions::ExtensionResource ext_resource =
      extensions::IconsInfo::GetIconResource(extension, size_hint_in_px,
                                             ExtensionIconSet::MATCH_BIGGER);

  if (extension && extension->location() ==
                       extensions::mojom::ManifestLocation::kComponent) {
    int resource_id = 0;
    const extensions::ComponentExtensionResourceManager* manager =
        extensions::ExtensionsBrowserClient::Get()
            ->GetComponentExtensionResourceManager();
    if (manager &&
        manager->IsComponentExtensionResource(
            extension->path(), ext_resource.relative_path(), &resource_id)) {
      base::StringPiece data =
          ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
              resource_id);
      std::move(compressed_data_callback)
          .Run(std::vector<uint8_t>(data.begin(), data.end()));
      return;
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CompressedDataFromResource, std::move(ext_resource)),
      std::move(compressed_data_callback));
}

base::Optional<IconPurpose> GetIconPurpose(
    const std::string& web_app_id,
    const web_app::AppIconManager& icon_manager,
    int size_hint_in_dip) {
  // Get the max supported pixel size.
  int max_icon_size_in_px = 0;
  for (auto scale_factor : ui::GetSupportedScaleFactors()) {
    const gfx::Size icon_size_in_px =
        gfx::ScaleToFlooredSize(gfx::Size(size_hint_in_dip, size_hint_in_dip),
                                ui::GetScaleForScaleFactor(scale_factor));
    DCHECK_EQ(icon_size_in_px.width(), icon_size_in_px.height());
    if (max_icon_size_in_px < icon_size_in_px.width()) {
      max_icon_size_in_px = icon_size_in_px.width();
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon) &&
      icon_manager.HasSmallestIcon(web_app_id, {IconPurpose::MASKABLE},
                                   max_icon_size_in_px)) {
    return base::make_optional(IconPurpose::MASKABLE);
  }
#endif

  if (icon_manager.HasSmallestIcon(web_app_id, {IconPurpose::ANY},
                                   max_icon_size_in_px)) {
    return base::make_optional(IconPurpose::ANY);
  }

  return base::nullopt;
}

// This pipeline is meant to:
// * Simplify loading icons, as things like effects and type are common
//   to all loading.
// * Allow the caller to halt the process by destructing the loader at any time,
// * Allow easy additions to the pipeline if necessary (like new effects or
// backups).
// Must be created & run from the UI thread.
class IconLoadingPipeline : public base::RefCounted<IconLoadingPipeline> {
 public:
  static const int kFaviconFallbackImagePx =
      extension_misc::EXTENSION_ICON_BITTY;

  IconLoadingPipeline(apps::mojom::IconType icon_type,
                      int size_hint_in_dip,
                      bool is_placeholder_icon,
                      apps::IconEffects icon_effects,
                      int fallback_icon_resource,
                      apps::mojom::Publisher::LoadIconCallback callback)
      : IconLoadingPipeline(icon_type,
                            size_hint_in_dip,
                            is_placeholder_icon,
                            icon_effects,
                            fallback_icon_resource,
                            base::OnceCallback<void(
                                apps::mojom::Publisher::LoadIconCallback)>(),
                            std::move(callback)) {}

  IconLoadingPipeline(
      apps::mojom::IconType icon_type,
      int size_hint_in_dip,
      bool is_placeholder_icon,
      apps::IconEffects icon_effects,
      int fallback_icon_resource,
      base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
          fallback,
      apps::mojom::Publisher::LoadIconCallback callback)
      : icon_type_(icon_type),
        size_hint_in_dip_(size_hint_in_dip),
        is_placeholder_icon_(is_placeholder_icon),
        icon_effects_(icon_effects),
        fallback_icon_resource_(fallback_icon_resource),
        callback_(std::move(callback)),
        fallback_callback_(std::move(fallback)) {
    icon_size_in_px_ = apps_util::ConvertDipToPx(
        size_hint_in_dip, /*quantize_to_supported_scale_factor=*/true);
    // Both px and dip sizes are integers but the scale factor is fractional.
    icon_scale_ = static_cast<float>(icon_size_in_px_) / size_hint_in_dip;
  }

  IconLoadingPipeline(
      int size_hint_in_dip,
      base::OnceCallback<void(const gfx::ImageSkia& icon)> callback)
      : size_hint_in_dip_(size_hint_in_dip),
        image_skia_callback_(std::move(callback)) {}

  explicit IconLoadingPipeline(
      base::OnceCallback<void(const std::vector<gfx::ImageSkia>& icons)>
          callback)
      : arc_activity_icons_callback_(std::move(callback)) {}

  void LoadWebAppIcon(const std::string& web_app_id,
                      const GURL& launch_url,
                      const web_app::AppIconManager& icon_manager,
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  friend class base::RefCounted<IconLoadingPipeline>;

  ~IconLoadingPipeline() {
    if (!callback_.is_null()) {
      std::move(callback_).Run(apps::mojom::IconValue::New());
    }
    if (!image_skia_callback_.is_null()) {
      std::move(image_skia_callback_).Run(gfx::ImageSkia());
    }
    if (!arc_activity_icons_callback_.is_null()) {
      std::move(arc_activity_icons_callback_)
          .Run(std::vector<gfx::ImageSkia>());
    }
  }

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

  void CompleteWithCompressed(std::vector<uint8_t> data);

  void CompleteWithImageSkia(gfx::ImageSkia image);

  void OnReadWebAppIcon(std::map<int, SkBitmap> icon_bitmaps);

  void MaybeLoadFallbackOrCompleteEmpty();

  apps::mojom::IconType icon_type_;

  int size_hint_in_dip_ = 0;
  int icon_size_in_px_ = 0;
  // The scale factor the icon is intended for. See gfx::ImageSkiaRep::scale
  // comments.
  float icon_scale_ = 0.0f;
  // A scale factor to take as input for the IconType::kCompressed response. See
  // gfx::ImageSkia::GetRepresentation() comments.
  float icon_scale_for_compressed_response_ = 1.0f;

  bool is_placeholder_icon_;
  apps::IconEffects icon_effects_;

  // If |fallback_favicon_url_| is populated, then the favicon service is the
  // first fallback method attempted in MaybeLoadFallbackOrCompleteEmpty().
  // These members are only populated from LoadWebAppIcon or LoadExtensionIcon.
  GURL fallback_favicon_url_;
  Profile* profile_ = nullptr;

  // If |fallback_icon_resource_| is not |kInvalidIconResource|, then it is the
  // second fallback method attempted in MaybeLoadFallbackOrCompleteEmpty()
  // (after the favicon service).
  int fallback_icon_resource_;

  apps::mojom::Publisher::LoadIconCallback callback_;

  // A custom fallback operation to try.
  base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
      fallback_callback_;

  base::CancelableTaskTracker cancelable_task_tracker_;

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

void IconLoadingPipeline::LoadWebAppIcon(
    const std::string& web_app_id,
    const GURL& launch_url,
    const web_app::AppIconManager& icon_manager,
    Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  fallback_favicon_url_ = launch_url;
  profile_ = profile;

  // In all other callpaths MaybeApplyEffectsAndComplete() uses
  // |icon_scale_for_compressed_response_| to apps::EncodeImageToPngBytes(). In
  // most cases IconLoadingPipeline always uses the 1.0 intended icon scale
  // factor as an intermediate representation to be compressed and returned.
  // TODO(crbug.com/1112737): Investigate how to unify it and set
  // |icon_scale_for_compressed_response_| value in IconLoadingPipeline()
  // constructor.
  icon_scale_for_compressed_response_ = icon_scale_;

  base::Optional<IconPurpose> icon_purpose_to_read =
      GetIconPurpose(web_app_id, icon_manager, size_hint_in_dip_);

  if (!icon_purpose_to_read.has_value()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }

  // Per https://www.w3.org/TR/appmanifest/#icon-masks, we apply a white
  // background in case the maskable icon contains transparent pixels in its
  // safe zone, and clear the standard icon effect, apply the mask to the icon
  // without shrinking it.
  if (icon_purpose_to_read.value() == IconPurpose::MASKABLE) {
    icon_effects_ &= ~apps::IconEffects::kCrOsStandardIcon;
    icon_effects_ |= apps::IconEffects::kCrOsStandardBackground;
    icon_effects_ |= apps::IconEffects::kCrOsStandardMask;
  }

  switch (icon_type_) {
    case apps::mojom::IconType::kCompressed:
      if (icon_effects_ == apps::IconEffects::kNone &&
          *icon_purpose_to_read == IconPurpose::ANY) {
        // Only read IconPurpose::ANY icons compressed as other purposes would
        // need to be uncompressed to apply icon effects.
        icon_manager.ReadSmallestCompressedIconAny(
            web_app_id, icon_size_in_px_,
            base::BindOnce(&IconLoadingPipeline::CompleteWithCompressed,
                           base::WrapRefCounted(this)));
        return;
      }
      FALLTHROUGH;
    case apps::mojom::IconType::kUncompressed:
      if (icon_type_ == apps::mojom::IconType::kUncompressed) {
        // For uncompressed icon, apply the resize and pad effect.
        icon_effects_ |= apps::IconEffects::kResizeAndPad;

        // For uncompressed icon, clear the standard icon effects: kBackground
        // and kMask.
        icon_effects_ &= ~apps::IconEffects::kCrOsStandardBackground;
        icon_effects_ &= ~apps::IconEffects::kCrOsStandardMask;
      }
      FALLTHROUGH;
    case apps::mojom::IconType::kStandard: {
      // If |icon_effects| are requested, we must always load the
      // uncompressed image to apply the icon effects, and then re-encode the
      // image if the compressed icon is requested.
      std::vector<int> icon_pixel_sizes;
      for (auto scale_factor : ui::GetSupportedScaleFactors()) {
        auto size_and_purpose = icon_manager.FindIconMatchBigger(
            web_app_id, {*icon_purpose_to_read},
            gfx::ScaleToFlooredSize(
                gfx::Size(size_hint_in_dip_, size_hint_in_dip_),
                ui::GetScaleForScaleFactor(scale_factor))
                .width());
        DCHECK(size_and_purpose.has_value());
        if (!base::Contains(icon_pixel_sizes, size_and_purpose->size_px)) {
          icon_pixel_sizes.emplace_back(size_and_purpose->size_px);
        }
      }
      DCHECK(!icon_pixel_sizes.empty());

      icon_manager.ReadIcons(
          web_app_id, *icon_purpose_to_read, icon_pixel_sizes,
          base::BindOnce(&IconLoadingPipeline::OnReadWebAppIcon,
                         base::WrapRefCounted(this)));

      return;
    }
    case apps::mojom::IconType::kUnknown:
      MaybeLoadFallbackOrCompleteEmpty();
      return;
  }
  NOTREACHED();
}

void IconLoadingPipeline::LoadExtensionIcon(
    const extensions::Extension* extension,
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!extension) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }

  fallback_favicon_url_ =
      extensions::AppLaunchInfo::GetFullLaunchURL(extension);
  profile_ = Profile::FromBrowserContext(context);
  switch (icon_type_) {
    case apps::mojom::IconType::kCompressed:
      // For compressed icons with no |icon_effects|, serve the
      // already-compressed bytes.
      if (icon_effects_ == apps::IconEffects::kNone) {
        // For the kUncompressed case, RunCallbackWithUncompressedImage
        // calls extensions::ImageLoader::LoadImageAtEveryScaleFactorAsync,
        // which already handles that distinction. We can't use
        // LoadImageAtEveryScaleFactorAsync here, because the caller has asked
        // for compressed icons (i.e. PNG-formatted data), not uncompressed
        // (i.e. a gfx::ImageSkia).
        LoadCompressedDataFromExtension(
            extension, icon_size_in_px_,
            base::BindOnce(&IconLoadingPipeline::CompleteWithCompressed,
                           base::WrapRefCounted(this)));
        return;
      }
      FALLTHROUGH;
    case apps::mojom::IconType::kUncompressed:
      FALLTHROUGH;
    case apps::mojom::IconType::kStandard:
      // If |icon_effects| are requested, we must always load the
      // uncompressed image to apply the icon effects, and then re-encode
      // the image if the compressed icon is requested.
      extensions::ImageLoader::Get(context)->LoadImageAtEveryScaleFactorAsync(
          extension, gfx::Size(size_hint_in_dip_, size_hint_in_dip_),
          ImageToImageSkia(
              base::BindOnce(&IconLoadingPipeline::MaybeApplyEffectsAndComplete,
                             base::WrapRefCounted(this))));
      return;
    case apps::mojom::IconType::kUnknown:
      break;
  }

  MaybeLoadFallbackOrCompleteEmpty();
}

void IconLoadingPipeline::LoadCompressedIconFromFile(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // For the compressed icon, MaybeApplyEffectsAndComplete() uses
  // |icon_scale_for_compressed_response_| to apps::EncodeImageToPngBytes(). So
  // set |icon_scale_for_compressed_response_| to match |icon_scale_|, which is
  // used to decode the icon.
  icon_scale_for_compressed_response_ = icon_scale_;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadFileAsCompressedData, path),
      apps::CompressedDataToImageSkiaCallback(
          base::BindOnce(&IconLoadingPipeline::MaybeApplyEffectsAndComplete,
                         base::WrapRefCounted(this)),
          icon_scale_));
}

void IconLoadingPipeline::LoadIconFromCompressedData(
    const std::string& compressed_icon_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // For the compressed icon, MaybeApplyEffectsAndComplete() uses
  // |icon_scale_for_compressed_response_| to apps::EncodeImageToPngBytes(). So
  // set |icon_scale_for_compressed_response_| to match |icon_scale_|, which is
  // used to decode the icon.
  icon_scale_for_compressed_response_ = icon_scale_;

  std::vector<uint8_t> data(compressed_icon_data.begin(),
                            compressed_icon_data.end());
  apps::CompressedDataToImageSkiaCallback(
      base::BindOnce(&IconLoadingPipeline::MaybeApplyEffectsAndComplete,
                     base::WrapRefCounted(this)),
      icon_scale_)
      .Run(std::move(data));
}

void IconLoadingPipeline::LoadIconFromResource(int icon_resource) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (icon_resource == IDR_LOGO_CROSTINI_DEFAULT ||
      icon_resource == IDR_APP_DEFAULT_ICON) {
    // For the Crostini penguin icon, clear the standard icon effects, and use
    // the raw icon.
    //
    // For the default icon, use the raw icon, because the standard icon image
    // convert could break the test cases.
    icon_effects_ &= ~apps::IconEffects::kCrOsStandardIcon;
  }
#endif

  if (icon_resource == kInvalidIconResource) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }

  switch (icon_type_) {
    case apps::mojom::IconType::kCompressed:
      // For compressed icons with no |icon_effects|, serve the
      // already-compressed bytes.
      if (icon_effects_ == apps::IconEffects::kNone) {
        base::StringPiece data =
            ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                icon_resource);
        CompleteWithCompressed(std::vector<uint8_t>(data.begin(), data.end()));
        return;
      }
      FALLTHROUGH;
    case apps::mojom::IconType::kUncompressed:
      FALLTHROUGH;
    case apps::mojom::IconType::kStandard: {
      // For compressed icons with |icon_effects|, or for uncompressed
      // icons, we load the uncompressed image, apply the icon effects, and
      // then re-encode the image if necessary.

      // Get the ImageSkia for the resource. The ui::ResourceBundle shared
      // instance already caches ImageSkia's, but caches the unscaled
      // versions. The |cache| here caches scaled versions, keyed by the
      // pair (resource_id, size_hint_in_dip).
      gfx::ImageSkia scaled;
      std::map<std::pair<int, int>, gfx::ImageSkia>& cache =
          GetResourceIconCache();
      const auto cache_key = std::make_pair(icon_resource, size_hint_in_dip_);
      const auto cache_iter = cache.find(cache_key);
      if (cache_iter != cache.end()) {
        scaled = cache_iter->second;
      } else {
        gfx::ImageSkia* unscaled =
            ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                icon_resource);
        scaled = gfx::ImageSkiaOperations::CreateResizedImage(
            *unscaled, skia::ImageOperations::RESIZE_BEST,
            gfx::Size(size_hint_in_dip_, size_hint_in_dip_));
        cache.insert(std::make_pair(cache_key, scaled));
      }

      // Apply icon effects, re-encode if necessary and run the callback.
      MaybeApplyEffectsAndComplete(scaled);
      return;
    }
    case apps::mojom::IconType::kUnknown:
      break;
  }
  MaybeLoadFallbackOrCompleteEmpty();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void IconLoadingPipeline::LoadArcIconPngData(
    const std::vector<uint8_t>& icon_png_data) {
  arc_icon_decode_request_ = CreateArcIconDecodeRequest(
      base::BindOnce(&IconLoadingPipeline::ApplyBackgroundAndMask,
                     base::WrapRefCounted(this)),
      icon_png_data);
}

void IconLoadingPipeline::LoadCompositeImages(
    const std::vector<uint8_t>& foreground_data,
    const std::vector<uint8_t>& background_data) {
  arc_foreground_icon_decode_request_ = CreateArcIconDecodeRequest(
      base::BindOnce(&IconLoadingPipeline::CompositeImagesAndApplyMask,
                     base::WrapRefCounted(this), true /* is_foreground */),
      foreground_data);

  arc_background_icon_decode_request_ = CreateArcIconDecodeRequest(
      base::BindOnce(&IconLoadingPipeline::CompositeImagesAndApplyMask,
                     base::WrapRefCounted(this), false /* is_foreground */),
      background_data);
}

void IconLoadingPipeline::LoadArcActivityIcons(
    const std::vector<arc::mojom::ActivityIconPtr>& icons) {
  arc_activity_icons_.resize(icons.size());
  DCHECK_EQ(0U, count_);
  for (size_t i = 0; i < icons.size(); i++) {
    if (!icons[i] || !icons[i]->icon_png_data) {
      ++count_;
      continue;
    }

    constexpr size_t kMaxIconSizeInPx = 200;
    if (icons[i]->width > kMaxIconSizeInPx ||
        icons[i]->height > kMaxIconSizeInPx || icons[i]->width == 0 ||
        icons[i]->height == 0) {
      ++count_;
      continue;
    }

    apps::ArcRawIconPngDataToImageSkia(
        std::move(icons[i]->icon_png_data), icons[i]->width,
        base::BindOnce(&IconLoadingPipeline::OnArcActivityIconLoaded,
                       base::WrapRefCounted(this), &arc_activity_icons_[i]));
  }

  if (count_ == arc_activity_icons_.size() && !image_skia_callback_.is_null()) {
    std::move(arc_activity_icons_callback_).Run(arc_activity_icons_);
  }
}

std::unique_ptr<arc::IconDecodeRequest>
IconLoadingPipeline::CreateArcIconDecodeRequest(
    base::OnceCallback<void(const gfx::ImageSkia& icon)> callback,
    const std::vector<uint8_t>& icon_png_data) {
  std::unique_ptr<arc::IconDecodeRequest> arc_icon_decode_request =
      std::make_unique<arc::IconDecodeRequest>(std::move(callback),
                                               size_hint_in_dip_);
  arc_icon_decode_request->StartWithOptions(icon_png_data);
  return arc_icon_decode_request;
}

void IconLoadingPipeline::ApplyBackgroundAndMask(const gfx::ImageSkia& image) {
  std::move(image_skia_callback_)
      .Run(gfx::ImageSkiaOperations::CreateResizedImage(
          apps::ApplyBackgroundAndMask(image),
          skia::ImageOperations::RESIZE_LANCZOS3,
          gfx::Size(size_hint_in_dip_, size_hint_in_dip_)));
}

void IconLoadingPipeline::CompositeImagesAndApplyMask(
    bool is_foreground,
    const gfx::ImageSkia& image) {
  if (is_foreground) {
    foreground_is_set_ = true;
    foreground_image_ = image;
  } else {
    background_is_set_ = true;
    background_image_ = image;
  }

  if (!foreground_is_set_ || !background_is_set_ ||
      image_skia_callback_.is_null()) {
    return;
  }

  if (foreground_image_.isNull() || background_image_.isNull()) {
    std::move(image_skia_callback_).Run(gfx::ImageSkia());
    return;
  }

  std::move(image_skia_callback_)
      .Run(gfx::ImageSkiaOperations::CreateResizedImage(
          apps::CompositeImagesAndApplyMask(foreground_image_,
                                            background_image_),
          skia::ImageOperations::RESIZE_BEST,
          gfx::Size(size_hint_in_dip_, size_hint_in_dip_)));
}

void IconLoadingPipeline::OnArcActivityIconLoaded(
    gfx::ImageSkia* arc_activity_icon,
    const gfx::ImageSkia& icon) {
  DCHECK(arc_activity_icon);
  ++count_;
  *arc_activity_icon = icon;

  if (count_ == arc_activity_icons_.size() &&
      !arc_activity_icons_callback_.is_null()) {
    std::move(arc_activity_icons_callback_).Run(arc_activity_icons_);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void IconLoadingPipeline::MaybeApplyEffectsAndComplete(
    const gfx::ImageSkia image) {
  if (image.isNull()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }
  gfx::ImageSkia processed_image = image;

  // Apply the icon effects on the uncompressed data. If the caller requests
  // an uncompressed icon, return the uncompressed result; otherwise, encode
  // the icon to a compressed icon, return the compressed result.
  if (icon_effects_) {
    apps::ApplyIconEffects(icon_effects_, size_hint_in_dip_, &processed_image);
  }

  if (icon_type_ == apps::mojom::IconType::kUncompressed ||
      icon_type_ == apps::mojom::IconType::kStandard) {
    CompleteWithImageSkia(processed_image);
    return;
  }

  processed_image.MakeThreadSafe();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&apps::EncodeImageToPngBytes, processed_image,
                     icon_scale_for_compressed_response_),
      base::BindOnce(&IconLoadingPipeline::CompleteWithCompressed,
                     base::WrapRefCounted(this)));
}

void IconLoadingPipeline::CompleteWithCompressed(std::vector<uint8_t> data) {
  DCHECK_EQ(icon_type_, apps::mojom::IconType::kCompressed);
  if (data.empty()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_type = apps::mojom::IconType::kCompressed;
  iv->compressed = std::move(data);
  iv->is_placeholder_icon = is_placeholder_icon_;
  std::move(callback_).Run(std::move(iv));
}

void IconLoadingPipeline::CompleteWithImageSkia(gfx::ImageSkia image) {
  DCHECK_NE(icon_type_, apps::mojom::IconType::kCompressed);
  DCHECK_NE(icon_type_, apps::mojom::IconType::kUnknown);
  if (image.isNull()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_type = icon_type_;
  iv->uncompressed = std::move(image);
  iv->is_placeholder_icon = is_placeholder_icon_;
  std::move(callback_).Run(std::move(iv));
}

// Callback for reading uncompressed web app icons.
void IconLoadingPipeline::OnReadWebAppIcon(
    std::map<int, SkBitmap> icon_bitmaps) {
  if (icon_bitmaps.empty()) {
    MaybeApplyEffectsAndComplete(gfx::ImageSkia());
    return;
  }

  gfx::ImageSkia image_skia;
  auto it = icon_bitmaps.begin();
  for (auto scale_factor : ui::GetSupportedScaleFactors()) {
    float icon_scale = ui::GetScaleForScaleFactor(scale_factor);
    int icon_size_in_px =
        gfx::ScaleToFlooredSize(gfx::Size(size_hint_in_dip_, size_hint_in_dip_),
                                icon_scale)
            .width();

    while (it != icon_bitmaps.end() && it->first < icon_size_in_px) {
      ++it;
    }

    if (it == icon_bitmaps.end() || it->second.empty()) {
      MaybeApplyEffectsAndComplete(gfx::ImageSkia());
      return;
    }

    SkBitmap bitmap = it->second;

    // Resize |bitmap| to match |icon_scale|.
    if (bitmap.width() != icon_size_in_px) {
      bitmap = skia::ImageOperations::Resize(
          bitmap, skia::ImageOperations::RESIZE_LANCZOS3, icon_size_in_px,
          icon_size_in_px);
    }

    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, icon_scale));
  }
  DCHECK_EQ(image_skia.image_reps().size(),
            ui::GetSupportedScaleFactors().size());
  MaybeApplyEffectsAndComplete(image_skia);
}

void IconLoadingPipeline::MaybeLoadFallbackOrCompleteEmpty() {
  if (fallback_favicon_url_.is_valid() &&
      icon_size_in_px_ == kFaviconFallbackImagePx) {
    GURL favicon_url = fallback_favicon_url_;
    // Reset to avoid infinite loops.
    fallback_favicon_url_ = GURL();
    favicon::FaviconService* favicon_service =
        FaviconServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (favicon_service) {
      favicon_service->GetRawFaviconForPageURL(
          favicon_url, {favicon_base::IconType::kFavicon}, gfx::kFaviconSize,
          /*fallback_to_host=*/false,
          FaviconResultToImageSkia(
              base::BindOnce(&IconLoadingPipeline::MaybeApplyEffectsAndComplete,
                             base::WrapRefCounted(this)),
              icon_scale_),
          &cancelable_task_tracker_);
      return;
    }
  }

  if (fallback_callback_) {
    // Wrap the result of |fallback_callback_| in another callback instead of
    // passing it to |callback_| directly so we can catch failures and try other
    // things.
    apps::mojom::Publisher::LoadIconCallback fallback_adaptor = base::BindOnce(
        [](scoped_refptr<IconLoadingPipeline> pipeline,
           apps::mojom::IconValuePtr ptr) {
          if (!ptr.is_null()) {
            std::move(pipeline->callback_).Run(std::move(ptr));
          } else {
            pipeline->MaybeLoadFallbackOrCompleteEmpty();
          }
        },
        base::WrapRefCounted(this));

    // Wrap this to ensure the fallback callback doesn't forget to call it.
    fallback_adaptor = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
        std::move(fallback_adaptor), nullptr);

    std::move(fallback_callback_).Run(std::move(fallback_adaptor));
    // |fallback_callback_| is null at this point, so if we get reinvoked then
    // we won't try this fallback again.

    return;
  }

  if (fallback_icon_resource_ != kInvalidIconResource) {
    int icon_resource = fallback_icon_resource_;
    // Resetting default icon resource to ensure no infinite loops.
    fallback_icon_resource_ = kInvalidIconResource;
    LoadIconFromResource(icon_resource);
    return;
  }

  std::move(callback_).Run(apps::mojom::IconValue::New());
}

}  // namespace

namespace apps {

base::OnceCallback<void(std::vector<uint8_t> compressed_data)>
CompressedDataToImageSkiaCallback(
    base::OnceCallback<void(gfx::ImageSkia)> callback,
    float icon_scale) {
  return base::BindOnce(
      [](base::OnceCallback<void(gfx::ImageSkia)> callback, float icon_scale,
         std::vector<uint8_t> compressed_data) {
        if (compressed_data.empty()) {
          std::move(callback).Run(gfx::ImageSkia());
          return;
        }
        // DecompressToSkBitmap is a CPU intensive task that must not run on the
        // UI thread, so post the processing over to the thread pool.
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(
                [](std::vector<uint8_t> compressed_data, float icon_scale) {
                  return SkBitmapToImageSkia(
                      DecompressToSkBitmap(compressed_data.data(),
                                           compressed_data.size()),
                      icon_scale);
                },
                std::move(compressed_data), icon_scale),
            std::move(callback));
      },
      std::move(callback), icon_scale);
}

std::vector<uint8_t> EncodeImageToPngBytes(const gfx::ImageSkia image,
                                           float rep_icon_scale) {
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

#if BUILDFLAG(IS_CHROMEOS_ASH)

gfx::ImageSkia LoadMaskImage(const ScaleToSize& scale_to_size) {
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
  return gfx::ImageSkiaOperations::CreateButtonBackground(
      SK_ColorWHITE, image, LoadMaskImage(GetScaleToSize(image)));
}

gfx::ImageSkia CompositeImagesAndApplyMask(
    const gfx::ImageSkia& foreground_image,
    const gfx::ImageSkia& background_image) {
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
  if (!icon) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  // For non-adaptive icons, add the white color background, and apply the mask.
  if (!icon->is_adaptive_icon) {
    base::UmaHistogramBoolean("Arc.AdaptiveIconLoad.FromNonArcAppIcon", false);

    if (!icon->icon_png_data.has_value()) {
      std::move(callback).Run(gfx::ImageSkia());
      return;
    }

    scoped_refptr<IconLoadingPipeline> icon_loader =
        base::MakeRefCounted<IconLoadingPipeline>(size_hint_in_dip,
                                                  std::move(callback));
    icon_loader->LoadArcIconPngData(icon->icon_png_data.value());
    return;
  }

  base::UmaHistogramBoolean("Arc.AdaptiveIconLoad.FromNonArcAppIcon", true);

  if (!icon->foreground_icon_png_data.has_value() ||
      icon->foreground_icon_png_data.value().empty() ||
      !icon->background_icon_png_data.has_value() ||
      icon->background_icon_png_data.value().empty()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  // For adaptive icons, composite the background and the foreground images
  // together, then applying the mask
  scoped_refptr<IconLoadingPipeline> icon_loader =
      base::MakeRefCounted<IconLoadingPipeline>(size_hint_in_dip,
                                                std::move(callback));
  icon_loader->LoadCompositeImages(
      std::move(icon->foreground_icon_png_data.value()),
      std::move(icon->background_icon_png_data.value()));
}

void ArcActivityIconsToImageSkias(
    const std::vector<arc::mojom::ActivityIconPtr>& icons,
    base::OnceCallback<void(const std::vector<gfx::ImageSkia>& icons)>
        callback) {
  if (icons.empty()) {
    std::move(callback).Run(std::vector<gfx::ImageSkia>{});
    return;
  }

  scoped_refptr<IconLoadingPipeline> icon_loader =
      base::MakeRefCounted<IconLoadingPipeline>(std::move(callback));
  icon_loader->LoadArcActivityIcons(icons);
}

gfx::ImageSkia ConvertSquareBitmapsToImageSkia(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps,
    IconEffects icon_effects,
    int size_hint_in_dip) {
  if (icon_bitmaps.empty()) {
    return gfx::ImageSkia{};
  }

  gfx::ImageSkia image_skia;
  auto it = icon_bitmaps.begin();

  for (ui::ScaleFactor scale_factor : ui::GetSupportedScaleFactors()) {
    float icon_scale = ui::GetScaleForScaleFactor(scale_factor);

    SquareSizePx icon_size_in_px =
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
    // TODO(crbug.com/1189994): All conversions in app_icon_factory.cc must
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
  ApplyIconEffects(icon_effects, size_hint_in_dip, &image_skia);

  return image_skia;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void ApplyIconEffects(IconEffects icon_effects,
                      int size_hint_in_dip,
                      gfx::ImageSkia* image_skia) {
  extensions::ChromeAppIcon::ResizeFunction resize_function;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (icon_effects & IconEffects::kResizeAndPad) {
    // TODO(crbug.com/826982): MD post-processing is not always applied: "See
    // legacy code:
    // https://cs.chromium.org/search/?q=ChromeAppIconLoader&type=cs In one
    // cases MD design is used in another not."
    resize_function =
        base::BindRepeating(&app_list::MaybeResizeAndPadIconForMd);
  }

  if (icon_effects & IconEffects::kCrOsStandardMask) {
    if (icon_effects & IconEffects::kCrOsStandardBackground) {
      *image_skia = apps::ApplyBackgroundAndMask(*image_skia);
    } else {
      auto mask_image = LoadMaskImage(GetScaleToSize(*image_skia));
      *image_skia =
          gfx::ImageSkiaOperations::CreateMaskedImage(*image_skia, mask_image);
    }
  }

  if (icon_effects & IconEffects::kCrOsStandardIcon) {
    *image_skia = apps::CreateStandardIconImage(*image_skia);
  }
#endif

  const bool from_bookmark = icon_effects & IconEffects::kRoundCorners;

  bool app_launchable = true;
  // Only one badge can be visible at a time.
  // Priority in which badges are applied (from the highest): Blocked > Paused >
  // Chrome. This means than when apps are disabled or paused app type
  // distinction information (Chrome vs Android) is lost.
  extensions::ChromeAppIcon::Badge badge_type =
      extensions::ChromeAppIcon::Badge::kNone;
  if (icon_effects & IconEffects::kBlocked) {
    badge_type = extensions::ChromeAppIcon::Badge::kBlocked;
    app_launchable = false;
  } else if (icon_effects & IconEffects::kPaused) {
    badge_type = extensions::ChromeAppIcon::Badge::kPaused;
    app_launchable = false;
  } else if (icon_effects & IconEffects::kChromeBadge) {
    badge_type = extensions::ChromeAppIcon::Badge::kChrome;
  }

  extensions::ChromeAppIcon::ApplyEffects(size_hint_in_dip, resize_function,
                                          app_launchable, from_bookmark,
                                          badge_type, image_skia);

  if (icon_effects & IconEffects::kPendingLocalLaunch) {
    color_utils::HSL shift = {-1, 0, 0.6};
    *image_skia =
        gfx::ImageSkiaOperations::CreateHSLShiftedImage(*image_skia, shift);
  }
}

void LoadIconFromExtension(apps::mojom::IconType icon_type,
                           int size_hint_in_dip,
                           content::BrowserContext* context,
                           const std::string& extension_id,
                           IconEffects icon_effects,
                           apps::mojom::Publisher::LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  constexpr bool is_placeholder_icon = false;
  scoped_refptr<IconLoadingPipeline> icon_loader =
      base::MakeRefCounted<IconLoadingPipeline>(
          icon_type, size_hint_in_dip, is_placeholder_icon, icon_effects,
          IDR_APP_DEFAULT_ICON, std::move(callback));
  icon_loader->LoadExtensionIcon(
      extensions::ExtensionRegistry::Get(context)->GetInstalledExtension(
          extension_id),
      context);
}

void LoadIconFromWebApp(content::BrowserContext* context,
                        apps::mojom::IconType icon_type,
                        int size_hint_in_dip,
                        const std::string& web_app_id,
                        IconEffects icon_effects,
                        apps::mojom::Publisher::LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(context);
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::Get(Profile::FromBrowserContext(context));

  DCHECK(web_app_provider);
  constexpr bool is_placeholder_icon = false;
  scoped_refptr<IconLoadingPipeline> icon_loader =
      base::MakeRefCounted<IconLoadingPipeline>(
          icon_type, size_hint_in_dip, is_placeholder_icon, icon_effects,
          IDR_APP_DEFAULT_ICON, std::move(callback));
  icon_loader->LoadWebAppIcon(
      web_app_id, web_app_provider->registrar().GetAppStartUrl(web_app_id),
      web_app_provider->icon_manager(), Profile::FromBrowserContext(context));
}

void LoadIconFromFileWithFallback(
    apps::mojom::IconType icon_type,
    int size_hint_in_dip,
    const base::FilePath& path,
    IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
        fallback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  constexpr bool is_placeholder_icon = false;

  scoped_refptr<IconLoadingPipeline> icon_loader =
      base::MakeRefCounted<IconLoadingPipeline>(
          icon_type, size_hint_in_dip, is_placeholder_icon, icon_effects,
          kInvalidIconResource, std::move(fallback), std::move(callback));
  icon_loader->LoadCompressedIconFromFile(path);
}

void LoadIconFromCompressedData(
    apps::mojom::IconType icon_type,
    int size_hint_in_dip,
    IconEffects icon_effects,
    const std::string& compressed_icon_data,
    apps::mojom::Publisher::LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  constexpr bool is_placeholder_icon = false;

  scoped_refptr<IconLoadingPipeline> icon_loader =
      base::MakeRefCounted<IconLoadingPipeline>(
          icon_type, size_hint_in_dip, is_placeholder_icon, icon_effects,
          kInvalidIconResource, std::move(callback));
  icon_loader->LoadIconFromCompressedData(compressed_icon_data);
}

void LoadIconFromResource(apps::mojom::IconType icon_type,
                          int size_hint_in_dip,
                          int resource_id,
                          bool is_placeholder_icon,
                          IconEffects icon_effects,
                          apps::mojom::Publisher::LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // There is no fallback icon for a resource.
  constexpr int fallback_icon_resource = 0;

  scoped_refptr<IconLoadingPipeline> icon_loader =
      base::MakeRefCounted<IconLoadingPipeline>(
          icon_type, size_hint_in_dip, is_placeholder_icon, icon_effects,
          fallback_icon_resource, std::move(callback));
  icon_loader->LoadIconFromResource(resource_id);
}

}  // namespace apps
