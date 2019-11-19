// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon_factory.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/md_icon_normalizer.h"
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

// Encode the ImageSkia to the compressed PNG data with the image's 1.0f scale
// factor representation. Return the encoded PNG data.
//
// This function should not be called on the UI thread.
std::vector<uint8_t> EncodeImage(const gfx::ImageSkia image) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<uint8_t> image_data;

  const gfx::ImageSkiaRep& image_skia_rep = image.GetRepresentation(1.0f);
  if (image_skia_rep.scale() != 1.0f) {
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

// Runs |callback| passing an IconValuePtr with a compressed image: a
// std::vector<uint8_t>.
//
// It will fall back to the |default_icon_resource| if the data is empty.
void RunCallbackWithCompressedData(
    int size_hint_in_dip,
    int default_icon_resource,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    std::vector<uint8_t> data) {
  if (!data.empty()) {
    apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
    iv->icon_compression = apps::mojom::IconCompression::kCompressed;
    iv->compressed = std::move(data);
    iv->is_placeholder_icon = is_placeholder_icon;
    std::move(callback).Run(std::move(iv));
    return;
  }
  if (default_icon_resource) {
    LoadIconFromResource(apps::mojom::IconCompression::kCompressed,
                         size_hint_in_dip, default_icon_resource,
                         is_placeholder_icon, icon_effects,
                         std::move(callback));
    return;
  }
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void RunCallbackWithCompressedDataFromExtension(
    const extensions::Extension* extension,
    int size_hint_in_dip,
    int default_icon_resource,
    bool is_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  // Load some component extensions' icons from statically compiled
  // resources (built into the Chrome binary), and other extensions'
  // icons (whether component extensions or otherwise) from files on
  // disk.
  //
  // For the kUncompressed case, RunCallbackWithUncompressedImage
  // calls extensions::ImageLoader::LoadImageAtEveryScaleFactorAsync, which
  // already handles that distinction. We can't use
  // LoadImageAtEveryScaleFactorAsync here, because the caller has asked for
  // compressed icons (i.e. PNG-formatted data), not uncompressed
  // (i.e. a gfx::ImageSkia).

  constexpr bool quantize_to_supported_scale_factor = true;
  int size_hint_in_px = apps_util::ConvertDipToPx(
      size_hint_in_dip, quantize_to_supported_scale_factor);
  extensions::ExtensionResource ext_resource =
      extensions::IconsInfo::GetIconResource(extension, size_hint_in_px,
                                             ExtensionIconSet::MATCH_BIGGER);

  if (extension && extension->location() == extensions::Manifest::COMPONENT) {
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
      RunCallbackWithCompressedData(
          size_hint_in_dip, default_icon_resource, is_placeholder_icon,
          apps::IconEffects::kNone, std::move(callback),
          std::vector<uint8_t>(data.begin(), data.end()));
      return;
    }
  }

  // Try and load data from the resource file.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CompressedDataFromResource, std::move(ext_resource)),
      base::BindOnce(&RunCallbackWithCompressedData, size_hint_in_dip,
                     default_icon_resource, is_placeholder_icon,
                     apps::IconEffects::kNone, std::move(callback)));
}

// Runs |callback| passing an IconValuePtr with an uncompressed image: an
// ImageSkia.
//
// It will fall back to the |default_icon_resource| if the image is null.
void RunCallbackWithImageSkia(int size_hint_in_dip,
                              int default_icon_resource,
                              bool is_placeholder_icon,
                              apps::IconEffects icon_effects,
                              apps::mojom::IconCompression icon_compression,
                              apps::mojom::Publisher::LoadIconCallback callback,
                              const gfx::ImageSkia image) {
  if (!image.isNull()) {
    gfx::ImageSkia processed_image = image;

    // Apply the icon effects on the uncompressed data. If the caller requests
    // an uncompressed icon, return the uncompressed result; otherwise, encode
    // the icon to a compressed icon, return the compressed result.
    if (icon_effects) {
      apps::ApplyIconEffects(icon_effects, size_hint_in_dip, &processed_image);
    }

    if (icon_compression == apps::mojom::IconCompression::kUncompressed) {
      apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
      iv->icon_compression = apps::mojom::IconCompression::kUncompressed;
      iv->uncompressed = processed_image;
      iv->is_placeholder_icon = is_placeholder_icon;
      std::move(callback).Run(std::move(iv));
      return;
    }

    processed_image.MakeThreadSafe();
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&EncodeImage, processed_image),
        base::BindOnce(&RunCallbackWithCompressedData, size_hint_in_dip,
                       default_icon_resource, is_placeholder_icon, icon_effects,
                       std::move(callback)));
    return;
  }

  if (default_icon_resource) {
    LoadIconFromResource(icon_compression, size_hint_in_dip,
                         default_icon_resource, is_placeholder_icon,
                         icon_effects, std::move(callback));
    return;
  }
  std::move(callback).Run(apps::mojom::IconValue::New());
}

// Given a gfx::Image |image|, runs |callback| passing image.AsImageSkia().
void RunCallbackWithImage(int size_hint_in_dip,
                          int default_icon_resource,
                          bool is_placeholder_icon,
                          apps::IconEffects icon_effects,
                          apps::mojom::IconCompression icon_compression,
                          apps::mojom::Publisher::LoadIconCallback callback,
                          const gfx::Image& image) {
  RunCallbackWithImageSkia(size_hint_in_dip, default_icon_resource,
                           is_placeholder_icon, icon_effects, icon_compression,
                           std::move(callback), image.AsImageSkia());
}

// Runs |callback| passing an IconValuePtr with an uncompressed image: a
// SkBitmap.
void RunCallbackWithSkBitmap(int size_hint_in_dip,
                             bool is_placeholder_icon,
                             apps::IconEffects icon_effects,
                             apps::mojom::IconCompression icon_compression,
                             apps::mojom::Publisher::LoadIconCallback callback,
                             const SkBitmap& bitmap) {
  constexpr int default_icon_resource = 0;
  gfx::ImageSkia image = gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, 0.0f));
  RunCallbackWithImageSkia(size_hint_in_dip, default_icon_resource,
                           is_placeholder_icon, icon_effects, icon_compression,
                           std::move(callback), image);
}

// Runs |callback| after converting (in a separate sandboxed process) from a
// std::vector<uint8_t> to a SkBitmap. It calls "fallback(callback)" if the
// data is empty.
void RunCallbackWithFallback(
    int size_hint_in_dip,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::IconCompression icon_compression,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)> fallback,
    std::vector<uint8_t> data) {
  if (data.empty()) {
    std::move(fallback).Run(std::move(callback));
    return;
  }

  if (icon_compression == apps::mojom::IconCompression::kCompressed) {
    constexpr int default_icon_resource = 0;
    RunCallbackWithCompressedData(size_hint_in_dip, default_icon_resource,
                                  is_placeholder_icon, icon_effects,
                                  std::move(callback), std::move(data));
    return;
  }

  data_decoder::DecodeImageIsolated(
      data, data_decoder::mojom::ImageCodec::DEFAULT, false,
      data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
      base::BindOnce(&RunCallbackWithSkBitmap, size_hint_in_dip,
                     is_placeholder_icon, icon_effects, icon_compression,
                     std::move(callback)));
}

}  // namespace

namespace apps {

void ApplyIconEffects(IconEffects icon_effects,
                      int size_hint_in_dip,
                      gfx::ImageSkia* image_skia) {
  extensions::ChromeAppIcon::ResizeFunction resize_function;
#if defined(OS_CHROMEOS)
  if (icon_effects & IconEffects::kResizeAndPad) {
    // TODO(crbug.com/826982): MD post-processing is not always applied: "See
    // legacy code:
    // https://cs.chromium.org/search/?q=ChromeAppIconLoader&type=cs In one
    // cases MD design is used in another not."
    resize_function =
        base::BindRepeating(&app_list::MaybeResizeAndPadIconForMd);
  }
#endif

  const bool apply_chrome_badge = icon_effects & IconEffects::kBadge;
  const bool app_launchable = !(icon_effects & IconEffects::kGray);
  const bool from_bookmark = icon_effects & IconEffects::kRoundCorners;

  extensions::ChromeAppIcon::ApplyEffects(size_hint_in_dip, resize_function,
                                          apply_chrome_badge, app_launchable,
                                          from_bookmark, image_skia);
}

void LoadIconFromExtension(apps::mojom::IconCompression icon_compression,
                           int size_hint_in_dip,
                           content::BrowserContext* context,
                           const std::string& extension_id,
                           IconEffects icon_effects,
                           apps::mojom::Publisher::LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  constexpr bool is_placeholder_icon = false;

  // This is the default icon for AppType::kExtension. Other app types might
  // use a different default icon, such as IDR_LOGO_CROSTINI_DEFAULT_192.
  constexpr int default_icon_resource = IDR_APP_DEFAULT_ICON;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(context)->GetInstalledExtension(
          extension_id);
  if (extension) {
    switch (icon_compression) {
      case apps::mojom::IconCompression::kUnknown:
        break;

      case apps::mojom::IconCompression::kUncompressed:
      case apps::mojom::IconCompression::kCompressed: {
        if (icon_compression == apps::mojom::IconCompression::kCompressed &&
            icon_effects == IconEffects::kNone) {
          RunCallbackWithCompressedDataFromExtension(
              extension, size_hint_in_dip, default_icon_resource,
              is_placeholder_icon, std::move(callback));
          return;
        }

        // If |icon_effects| are requested, we must always load the
        // uncompressed image to apply the icon effects, and then re-encode the
        // image if the compressed icon is requested.
        extensions::ImageLoader::Get(context)->LoadImageAtEveryScaleFactorAsync(
            extension, gfx::Size(size_hint_in_dip, size_hint_in_dip),
            base::BindOnce(&RunCallbackWithImage, size_hint_in_dip,
                           default_icon_resource, is_placeholder_icon,
                           icon_effects, icon_compression,
                           std::move(callback)));
        return;
      }
    }
  }

  // Fall back to the default_icon_resource.
  LoadIconFromResource(icon_compression, size_hint_in_dip,
                       default_icon_resource, is_placeholder_icon, icon_effects,
                       std::move(callback));
}

void LoadIconFromFileWithFallback(
    apps::mojom::IconCompression icon_compression,
    int size_hint_in_dip,
    const base::FilePath& path,
    IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
        fallback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  constexpr bool is_placeholder_icon = false;
  switch (icon_compression) {
    case apps::mojom::IconCompression::kUnknown:
      break;

    case apps::mojom::IconCompression::kUncompressed:
    case apps::mojom::IconCompression::kCompressed: {
      base::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ReadFileAsCompressedData, path),
          base::BindOnce(&RunCallbackWithFallback, size_hint_in_dip,
                         is_placeholder_icon, icon_effects, icon_compression,
                         std::move(callback), std::move(fallback)));
      return;
    }
  }

  std::move(callback).Run(apps::mojom::IconValue::New());
}

void LoadIconFromResource(apps::mojom::IconCompression icon_compression,
                          int size_hint_in_dip,
                          int resource_id,
                          bool is_placeholder_icon,
                          IconEffects icon_effects,
                          apps::mojom::Publisher::LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // This must be zero, to avoid a potential infinite loop if the
  // RunCallbackWithXxx functions could otherwise call back into
  // LoadIconFromResource.
  constexpr int default_icon_resource = 0;

  if (resource_id != 0) {
    switch (icon_compression) {
      case apps::mojom::IconCompression::kUnknown:
        break;

      case apps::mojom::IconCompression::kUncompressed:
      case apps::mojom::IconCompression::kCompressed: {
        // For compressed icons with no |icon_effects|, serve the
        // already-compressed bytes.
        if (icon_compression == apps::mojom::IconCompression::kCompressed &&
            icon_effects == IconEffects::kNone) {
          base::StringPiece data =
              ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                  resource_id);
          RunCallbackWithCompressedData(
              size_hint_in_dip, default_icon_resource, is_placeholder_icon,
              icon_effects, std::move(callback),
              std::vector<uint8_t>(data.begin(), data.end()));
          return;
        }

        // For compressed icons with |icon_effects|, or for uncompressed icons,
        // we load the uncompressed image, apply the icon effects, and then
        // re-encode the image if necessary.

        // Get the ImageSkia for the resource. The ui::ResourceBundle shared
        // instance already caches ImageSkia's, but caches the unscaled
        // versions. The |cache| here caches scaled versions, keyed by the pair
        // (resource_id, size_hint_in_dip).
        gfx::ImageSkia scaled;
        std::map<std::pair<int, int>, gfx::ImageSkia>& cache =
            GetResourceIconCache();
        const auto cache_key = std::make_pair(resource_id, size_hint_in_dip);
        const auto cache_iter = cache.find(cache_key);
        if (cache_iter != cache.end()) {
          scaled = cache_iter->second;
        } else {
          gfx::ImageSkia* unscaled =
              ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                  resource_id);
          scaled = gfx::ImageSkiaOperations::CreateResizedImage(
              *unscaled, skia::ImageOperations::RESIZE_BEST,
              gfx::Size(size_hint_in_dip, size_hint_in_dip));
          cache.insert(std::make_pair(cache_key, scaled));
        }

        // Apply icon effects, re-encode if necessary and run the callback.
        RunCallbackWithImageSkia(size_hint_in_dip, default_icon_resource,
                                 is_placeholder_icon, icon_effects,
                                 icon_compression, std::move(callback), scaled);
        return;
      }
    }
  }

  std::move(callback).Run(apps::mojom::IconValue::New());
}

}  // namespace apps
