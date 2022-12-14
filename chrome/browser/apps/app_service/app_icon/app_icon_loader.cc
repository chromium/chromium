// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_loader.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/ash/app_list/md_icon_normalizer.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#endif

namespace {

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

absl::optional<IconPurpose> GetIconPurpose(
    const std::string& web_app_id,
    const web_app::WebAppIconManager& icon_manager,
    int size_hint_in_dip) {
  // Get the max supported pixel size.
  int max_icon_size_in_px = 0;
  for (auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    const gfx::Size icon_size_in_px = gfx::ScaleToFlooredSize(
        gfx::Size(size_hint_in_dip, size_hint_in_dip),
        ui::GetScaleForResourceScaleFactor(scale_factor));
    DCHECK_EQ(icon_size_in_px.width(), icon_size_in_px.height());
    if (max_icon_size_in_px < icon_size_in_px.width()) {
      max_icon_size_in_px = icon_size_in_px.width();
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (icon_manager.HasSmallestIcon(web_app_id, {IconPurpose::MASKABLE},
                                   max_icon_size_in_px)) {
    return absl::make_optional(IconPurpose::MASKABLE);
  }
#endif

  if (icon_manager.HasSmallestIcon(web_app_id, {IconPurpose::ANY},
                                   max_icon_size_in_px)) {
    return absl::make_optional(IconPurpose::ANY);
  }

  return absl::nullopt;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
apps::IconValuePtr ApplyEffects(apps::IconEffects icon_effects,
                                int size_hint_in_dip,
                                apps::IconValuePtr iv,
                                gfx::ImageSkia mask_image) {
  base::AssertLongCPUWorkAllowed();

  if ((icon_effects & apps::IconEffects::kCrOsStandardMask) &&
      !mask_image.isNull()) {
    if (icon_effects & apps::IconEffects::kCrOsStandardBackground) {
      iv->uncompressed = gfx::ImageSkiaOperations::CreateButtonBackground(
          SK_ColorWHITE, iv->uncompressed, mask_image);
    } else {
      iv->uncompressed = gfx::ImageSkiaOperations::CreateMaskedImage(
          iv->uncompressed, mask_image);
    }
  }

  if (icon_effects & apps::IconEffects::kCrOsStandardIcon) {
    // We should never reapply the icon shaping logic.
    DCHECK(!(icon_effects & apps::IconEffects::kCrOsStandardMask));
    iv->uncompressed = apps::CreateStandardIconImage(iv->uncompressed);
  }

  if (icon_effects & apps::IconEffects::kMdIconStyle) {
    // TODO(crbug.com/826982): MD post-processing is not always applied: "See
    // legacy code:
    // https://cs.chromium.org/search/?q=ChromeAppIconLoader&type=cs In one
    // cases MD design is used in another not."
    app_list::MaybeResizeAndPadIconForMd(
        gfx::Size(size_hint_in_dip, size_hint_in_dip), &iv->uncompressed);
  }

  if (!iv->uncompressed.isNull()) {
    iv->uncompressed.MakeThreadSafe();
  }

  return iv;
}
#endif

}  // namespace

namespace apps {

AppIconLoader::AppIconLoader(IconType icon_type,
                             int size_hint_in_dip,
                             bool is_placeholder_icon,
                             apps::IconEffects icon_effects,
                             int fallback_icon_resource,
                             LoadIconCallback callback)
    : AppIconLoader(icon_type,
                    size_hint_in_dip,
                    is_placeholder_icon,
                    icon_effects,
                    fallback_icon_resource,
                    base::OnceCallback<void(LoadIconCallback)>(),
                    std::move(callback)) {}

AppIconLoader::AppIconLoader(
    IconType icon_type,
    int size_hint_in_dip,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    int fallback_icon_resource,
    base::OnceCallback<void(LoadIconCallback)> fallback,
    LoadIconCallback callback)
    : icon_type_(icon_type),
      size_hint_in_dip_(size_hint_in_dip),
      icon_size_in_px_(apps_util::ConvertDipToPx(
          size_hint_in_dip,
          /*quantize_to_supported_scale_factor=*/true)),
      // Both px and dip sizes are integers but the scale factor is fractional.
      icon_scale_(static_cast<float>(icon_size_in_px_) / size_hint_in_dip),
      is_placeholder_icon_(is_placeholder_icon),
      icon_effects_(icon_effects),
      fallback_icon_resource_(fallback_icon_resource),
      callback_(std::move(callback)),
      fallback_callback_(std::move(fallback)) {}

AppIconLoader::AppIconLoader(
    int size_hint_in_dip,
    base::OnceCallback<void(const gfx::ImageSkia& icon)> callback)
    : size_hint_in_dip_(size_hint_in_dip),
      image_skia_callback_(std::move(callback)) {}

AppIconLoader::AppIconLoader(
    base::OnceCallback<void(const std::vector<gfx::ImageSkia>& icons)> callback)
    : arc_activity_icons_callback_(std::move(callback)) {}

AppIconLoader::AppIconLoader(int size_hint_in_dip, LoadIconCallback callback)
    : size_hint_in_dip_(size_hint_in_dip), callback_(std::move(callback)) {}

AppIconLoader::~AppIconLoader() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(std::make_unique<IconValue>());
  }
  if (!image_skia_callback_.is_null()) {
    std::move(image_skia_callback_).Run(gfx::ImageSkia());
  }
  if (!arc_activity_icons_callback_.is_null()) {
    std::move(arc_activity_icons_callback_).Run(std::vector<gfx::ImageSkia>());
  }
}

void AppIconLoader::ApplyIconEffects(IconEffects icon_effects,
                                     IconValuePtr iv) {
  if (!iv || iv->uncompressed.isNull())
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!standard_icon_task_runner_) {
    standard_icon_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }

  gfx::ImageSkia mask_image;
  if (icon_effects & apps::IconEffects::kCrOsStandardMask) {
    mask_image = apps::LoadMaskImage(GetScaleToSize(iv->uncompressed));
    mask_image.MakeThreadSafe();
  }

  iv->uncompressed.MakeThreadSafe();

  standard_icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ApplyEffects, icon_effects, size_hint_in_dip_,
                     std::move(iv), mask_image),
      base::BindOnce(&AppIconLoader::ApplyBadges, base::WrapRefCounted(this),
                     icon_effects));
#else
  ApplyBadges(icon_effects, std::move(iv));
#endif
}

void AppIconLoader::ApplyBadges(IconEffects icon_effects, IconValuePtr iv) {
  const bool from_bookmark = icon_effects & apps::IconEffects::kRoundCorners;

  bool app_launchable = true;
  // Only one badge can be visible at a time.
  // Priority in which badges are applied (from the highest): Blocked > Paused >
  // Chrome. This means than when apps are disabled or paused app type
  // distinction information (Chrome vs Android) is lost.
  extensions::ChromeAppIcon::Badge badge_type =
      extensions::ChromeAppIcon::Badge::kNone;
  if (icon_effects & apps::IconEffects::kBlocked) {
    badge_type = extensions::ChromeAppIcon::Badge::kBlocked;
    app_launchable = false;
  } else if (icon_effects & apps::IconEffects::kPaused) {
    badge_type = extensions::ChromeAppIcon::Badge::kPaused;
    app_launchable = false;
  } else if (icon_effects & apps::IconEffects::kChromeBadge) {
    badge_type = extensions::ChromeAppIcon::Badge::kChrome;
  }

  extensions::ChromeAppIcon::ApplyEffects(
      size_hint_in_dip_, extensions::ChromeAppIcon::ResizeFunction(),
      app_launchable, from_bookmark, badge_type, &iv->uncompressed);

  std::move(callback_).Run(std::move(iv));
}

void AppIconLoader::LoadWebAppIcon(const std::string& web_app_id,
                                   const GURL& launch_url,
                                   web_app::WebAppIconManager& icon_manager,
                                   Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  fallback_favicon_url_ = launch_url;
  profile_ = profile;

  // In all other callpaths MaybeApplyEffectsAndComplete() uses
  // |icon_scale_for_compressed_response_| to apps::EncodeImageToPngBytes(). In
  // most cases AppIconLoader always uses the 1.0 intended icon scale
  // factor as an intermediate representation to be compressed and returned.
  // TODO(crbug.com/1112737): Investigate how to unify it and set
  // |icon_scale_for_compressed_response_| value in AppIconLoader()
  // constructor.
  icon_scale_for_compressed_response_ = icon_scale_;

  absl::optional<IconPurpose> icon_purpose_to_read =
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
    case IconType::kCompressed:
      [[fallthrough]];
    case IconType::kUncompressed:
      if (icon_type_ == apps::IconType::kUncompressed) {
        // For uncompressed icon, apply the resize and pad effect.
        icon_effects_ |= apps::IconEffects::kMdIconStyle;

        // For uncompressed icon, clear the standard icon effects: kBackground
        // and kMask.
        icon_effects_ &= ~apps::IconEffects::kCrOsStandardBackground;
        icon_effects_ &= ~apps::IconEffects::kCrOsStandardMask;
      }
      [[fallthrough]];
    case IconType::kStandard: {
      // We always load the uncompressed image to apply the icon effects or
      // resize the icon size, and then re-encode the image if the compressed
      // icon is requested.
      std::vector<int> icon_pixel_sizes;
      for (auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
        auto size_and_purpose = icon_manager.FindIconMatchBigger(
            web_app_id, {*icon_purpose_to_read},
            apps_util::ConvertDipToPxForScale(
                size_hint_in_dip_,
                ui::GetScaleForResourceScaleFactor(scale_factor)));
        DCHECK(size_and_purpose.has_value());
        if (!base::Contains(icon_pixel_sizes, size_and_purpose->size_px)) {
          icon_pixel_sizes.emplace_back(size_and_purpose->size_px);
        }
      }
      DCHECK(!icon_pixel_sizes.empty());

      icon_manager.ReadIcons(web_app_id, *icon_purpose_to_read,
                             icon_pixel_sizes,
                             base::BindOnce(&AppIconLoader::OnReadWebAppIcon,
                                            base::WrapRefCounted(this)));

      return;
    }
    case IconType::kUnknown:
      MaybeLoadFallbackOrCompleteEmpty();
      return;
  }
  NOTREACHED();
}

void AppIconLoader::LoadExtensionIcon(const extensions::Extension* extension,
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
    case IconType::kCompressed:
      [[fallthrough]];
    case IconType::kUncompressed:
      [[fallthrough]];
    case IconType::kStandard:
      // We always load the uncompressed image to apply the icon effects or
      // resize the icon size, and then re-encode the image if the compressed
      // icon is requested.
      extensions::ImageLoader::Get(context)->LoadImageAtEveryScaleFactorAsync(
          extension, gfx::Size(size_hint_in_dip_, size_hint_in_dip_),
          ImageToImageSkia(
              base::BindOnce(&AppIconLoader::MaybeApplyEffectsAndComplete,
                             base::WrapRefCounted(this))));
      return;
    case IconType::kUnknown:
      break;
  }

  MaybeLoadFallbackOrCompleteEmpty();
}

void AppIconLoader::LoadCompressedIconFromFile(const base::FilePath& path) {
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
          base::BindOnce(&AppIconLoader::MaybeApplyEffectsAndComplete,
                         base::WrapRefCounted(this)),
          icon_scale_));
}

void AppIconLoader::LoadIconFromCompressedData(
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
      base::BindOnce(&AppIconLoader::MaybeApplyEffectsAndComplete,
                     base::WrapRefCounted(this)),
      icon_scale_)
      .Run(std::move(data));
}

void AppIconLoader::LoadIconFromResource(int icon_resource) {
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
    case IconType::kCompressed:
      // For compressed icons with no |icon_effects|, serve the
      // already-compressed bytes.
      if (icon_effects_ == apps::IconEffects::kNone) {
        base::StringPiece data =
            ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                icon_resource);
        CompleteWithCompressed(/*is_maskable_icon=*/false,
                               std::vector<uint8_t>(data.begin(), data.end()));
        return;
      }
      [[fallthrough]];
    case IconType::kUncompressed:
      [[fallthrough]];
    case IconType::kStandard: {
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
    case IconType::kUnknown:
      break;
  }
  MaybeLoadFallbackOrCompleteEmpty();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AppIconLoader::LoadArcIconPngData(
    const std::vector<uint8_t>& icon_png_data) {
  arc_icon_decode_request_ = CreateArcIconDecodeRequest(
      base::BindOnce(&AppIconLoader::ApplyBackgroundAndMask,
                     base::WrapRefCounted(this)),
      icon_png_data);
}

void AppIconLoader::LoadCompositeImages(
    const std::vector<uint8_t>& foreground_data,
    const std::vector<uint8_t>& background_data) {
  arc_foreground_icon_decode_request_ = CreateArcIconDecodeRequest(
      base::BindOnce(&AppIconLoader::CompositeImagesAndApplyMask,
                     base::WrapRefCounted(this), true /* is_foreground */),
      foreground_data);

  arc_background_icon_decode_request_ = CreateArcIconDecodeRequest(
      base::BindOnce(&AppIconLoader::CompositeImagesAndApplyMask,
                     base::WrapRefCounted(this), false /* is_foreground */),
      background_data);
}

void AppIconLoader::LoadArcActivityIcons(
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
        base::BindOnce(&AppIconLoader::OnArcActivityIconLoaded,
                       base::WrapRefCounted(this), &arc_activity_icons_[i]));
  }

  if (count_ == arc_activity_icons_.size() && !image_skia_callback_.is_null()) {
    std::move(arc_activity_icons_callback_).Run(arc_activity_icons_);
  }
}

void AppIconLoader::GetWebAppCompressedIconData(
    const std::string& web_app_id,
    ui::ResourceScaleFactor scale_factor,
    web_app::WebAppIconManager& icon_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  absl::optional<IconPurpose> icon_purpose_to_read =
      GetIconPurpose(web_app_id, icon_manager, size_hint_in_dip_);

  if (!icon_purpose_to_read.has_value() || icon_type_ == IconType::kUnknown) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }

  icon_scale_ = ui::GetScaleForResourceScaleFactor(scale_factor);
  icon_size_in_px_ =
      apps_util::ConvertDipToPxForScale(size_hint_in_dip_, icon_scale_);

  auto size_and_purpose = icon_manager.FindIconMatchBigger(
      web_app_id, {*icon_purpose_to_read}, icon_size_in_px_);
  DCHECK(size_and_purpose.has_value());

  std::vector<int> icon_pixel_sizes;
  icon_pixel_sizes.emplace_back(size_and_purpose->size_px);
  icon_manager.ReadIcons(
      web_app_id, *icon_purpose_to_read, icon_pixel_sizes,
      base::BindOnce(&AppIconLoader::OnReadWebAppForCompressedIconData,
                     base::WrapRefCounted(this),
                     *icon_purpose_to_read == IconPurpose::MASKABLE));
}

void AppIconLoader::GetChromeAppCompressedIconData(
    const extensions::Extension* extension,
    content::BrowserContext* context,
    ui::ResourceScaleFactor scale_factor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!extension || icon_type_ == IconType::kUnknown) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }

  icon_scale_ = ui::GetScaleForResourceScaleFactor(scale_factor);
  profile_ = Profile::FromBrowserContext(context);
  extensions::ImageLoader::Get(context)->LoadImageAtEveryScaleFactorAsync(
      extension, gfx::Size(size_hint_in_dip_, size_hint_in_dip_),
      ImageToImageSkia(
          base::BindOnce(&AppIconLoader::OnReadChromeAppForCompressedIconData,
                         base::WrapRefCounted(this))));
}

std::unique_ptr<arc::IconDecodeRequest>
AppIconLoader::CreateArcIconDecodeRequest(
    base::OnceCallback<void(const gfx::ImageSkia& icon)> callback,
    const std::vector<uint8_t>& icon_png_data) {
  std::unique_ptr<arc::IconDecodeRequest> arc_icon_decode_request =
      std::make_unique<arc::IconDecodeRequest>(std::move(callback),
                                               size_hint_in_dip_);
  arc_icon_decode_request->StartWithOptions(icon_png_data);
  return arc_icon_decode_request;
}

void AppIconLoader::ApplyBackgroundAndMask(const gfx::ImageSkia& image) {
  std::move(image_skia_callback_)
      .Run(gfx::ImageSkiaOperations::CreateResizedImage(
          apps::ApplyBackgroundAndMask(image),
          skia::ImageOperations::RESIZE_LANCZOS3,
          gfx::Size(size_hint_in_dip_, size_hint_in_dip_)));
}

void AppIconLoader::CompositeImagesAndApplyMask(bool is_foreground,
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

void AppIconLoader::OnArcActivityIconLoaded(gfx::ImageSkia* arc_activity_icon,
                                            const gfx::ImageSkia& icon) {
  DCHECK(arc_activity_icon);
  ++count_;
  *arc_activity_icon = icon;
  arc_activity_icon->MakeThreadSafe();

  if (count_ == arc_activity_icons_.size() &&
      !arc_activity_icons_callback_.is_null()) {
    std::move(arc_activity_icons_callback_).Run(arc_activity_icons_);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void AppIconLoader::MaybeApplyEffectsAndComplete(const gfx::ImageSkia image) {
  if (image.isNull()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }

  auto iv = std::make_unique<IconValue>();
  iv->icon_type = icon_type_;
  iv->uncompressed = image;
  iv->is_placeholder_icon = is_placeholder_icon_;

  // Apply the icon effects on the uncompressed data. If the caller requests
  // an uncompressed icon, return the uncompressed result; otherwise, encode
  // the icon to a compressed icon, return the compressed result.
  if (icon_effects_) {
    apps::ApplyIconEffects(icon_effects_, size_hint_in_dip_, std::move(iv),
                           base::BindOnce(&AppIconLoader::CompleteWithIconValue,
                                          base::WrapRefCounted(this)));
    return;
  }

  CompleteWithIconValue(std::move(iv));
}

void AppIconLoader::CompleteWithCompressed(bool is_maskable_icon,
                                           std::vector<uint8_t> data) {
  if (data.empty()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }
  auto iv = std::make_unique<IconValue>();
  iv->icon_type = IconType::kCompressed;
  iv->compressed = std::move(data);
  iv->is_maskable_icon = is_maskable_icon;
  iv->is_placeholder_icon = is_placeholder_icon_;
  std::move(callback_).Run(std::move(iv));
}

void AppIconLoader::CompleteWithUncompressed(IconValuePtr iv) {
  DCHECK_NE(icon_type_, IconType::kCompressed);
  DCHECK_NE(icon_type_, IconType::kUnknown);
  if (iv->uncompressed.isNull()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }
  std::move(callback_).Run(std::move(iv));
}

void AppIconLoader::CompleteWithIconValue(IconValuePtr iv) {
  if (icon_type_ == IconType::kUncompressed ||
      icon_type_ == IconType::kStandard) {
    CompleteWithUncompressed(std::move(iv));
    return;
  }

  if (iv->uncompressed.isNull()) {
    // In browser tests, CompleteWithIconValue might be called by the system
    // shutdown process, but the apply icon effect process hasn't finished, so
    // the icon might be null. Return early here if the image is null, to
    // prevent calling MakeThreadSafe, which might cause the system crash due to
    // DCHECK error on image.
    CompleteWithCompressed(/*is_maskable_icon=*/false, std::vector<uint8_t>());
    return;
  }

  iv->uncompressed.MakeThreadSafe();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&apps::EncodeImageToPngBytes, iv->uncompressed,
                     icon_scale_for_compressed_response_),
      base::BindOnce(&AppIconLoader::CompleteWithCompressed,
                     base::WrapRefCounted(this), /*is_maskable_icon=*/false));
}

// Callback for reading uncompressed web app icons.
void AppIconLoader::OnReadWebAppIcon(std::map<int, SkBitmap> icon_bitmaps) {
  if (icon_bitmaps.empty()) {
    MaybeApplyEffectsAndComplete(gfx::ImageSkia());
    return;
  }

  gfx::ImageSkia image_skia;
  auto it = icon_bitmaps.begin();
  for (auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    float icon_scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    int icon_size_in_px =
        apps_util::ConvertDipToPxForScale(size_hint_in_dip_, icon_scale);

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
            ui::GetSupportedResourceScaleFactors().size());
  MaybeApplyEffectsAndComplete(image_skia);
}

void AppIconLoader::OnReadWebAppForCompressedIconData(
    bool is_maskable_icon,
    std::map<int, SkBitmap> icon_bitmaps) {
  if (icon_bitmaps.empty()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }

  SkBitmap bitmap = icon_bitmaps.begin()->second;

  // Resize |bitmap| to match |icon_scale|.
  bitmap = skia::ImageOperations::Resize(bitmap,
                                         skia::ImageOperations::RESIZE_LANCZOS3,
                                         icon_size_in_px_, icon_size_in_px_);

  gfx::ImageSkia image_skia;
  image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, icon_scale_));
  image_skia.MakeThreadSafe();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&apps::EncodeImageToPngBytes, image_skia, icon_scale_),
      base::BindOnce(&AppIconLoader::CompleteWithCompressed,
                     base::WrapRefCounted(this), is_maskable_icon));
}

void AppIconLoader::OnReadChromeAppForCompressedIconData(gfx::ImageSkia image) {
  if (image.isNull()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }

  image.MakeThreadSafe();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&apps::EncodeImageToPngBytes, image, icon_scale_),
      base::BindOnce(&AppIconLoader::CompleteWithCompressed,
                     base::WrapRefCounted(this), /*is_maskable_icon=*/false));
}

void AppIconLoader::MaybeLoadFallbackOrCompleteEmpty() {
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
              base::BindOnce(&AppIconLoader::MaybeApplyEffectsAndComplete,
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
    LoadIconCallback fallback_adaptor = base::BindOnce(
        [](scoped_refptr<AppIconLoader> icon_loader, IconValuePtr iv) {
          if (iv) {
            std::move(icon_loader->callback_).Run(std::move(iv));
          } else {
            icon_loader->MaybeLoadFallbackOrCompleteEmpty();
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

  std::move(callback_).Run(std::make_unique<IconValue>());
}

}  // namespace apps
