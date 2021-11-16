// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/mojom/personalization_app_mojom_traits.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

using MojomWallpaperLayout = ash::personalization_app::mojom::WallpaperLayout;
using MojomWallpaperType = ash::personalization_app::mojom::WallpaperType;
using MojomOnlineImageType = ash::personalization_app::mojom::OnlineImageType;

MojomWallpaperLayout
EnumTraits<MojomWallpaperLayout, ash::WallpaperLayout>::ToMojom(
    ash::WallpaperLayout input) {
  switch (input) {
    case ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER:
      return MojomWallpaperLayout::kCenter;
    case ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED:
      return MojomWallpaperLayout::kCenterCropped;
    case ash::WallpaperLayout::WALLPAPER_LAYOUT_STRETCH:
      return MojomWallpaperLayout::kStretch;
    case ash::WallpaperLayout::WALLPAPER_LAYOUT_TILE:
      return MojomWallpaperLayout::kTile;
    case ash::WallpaperLayout::NUM_WALLPAPER_LAYOUT:
      NOTREACHED();
      return MojomWallpaperLayout::kCenter;
  }
}

bool EnumTraits<MojomWallpaperLayout, ash::WallpaperLayout>::FromMojom(
    MojomWallpaperLayout input,
    ash::WallpaperLayout* output) {
  switch (input) {
    case MojomWallpaperLayout::kCenter:
      *output = ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER;
      return true;
    case MojomWallpaperLayout::kCenterCropped:
      *output = ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED;
      return true;
    case MojomWallpaperLayout::kStretch:
      *output = ash::WallpaperLayout::WALLPAPER_LAYOUT_STRETCH;
      return true;
    case MojomWallpaperLayout::kTile:
      *output = ash::WallpaperLayout::WALLPAPER_LAYOUT_TILE;
      return true;
  }
  NOTREACHED();
  return false;
}

MojomWallpaperType EnumTraits<MojomWallpaperType, ash::WallpaperType>::ToMojom(
    ash::WallpaperType input) {
  switch (input) {
    case ash::WallpaperType::kDaily:
      return MojomWallpaperType::kDaily;
    case ash::WallpaperType::kCustomized:
      return MojomWallpaperType::kCustomized;
    case ash::WallpaperType::kDefault:
      return MojomWallpaperType::kDefault;
    case ash::WallpaperType::kOnline:
      return MojomWallpaperType::kOnline;
    case ash::WallpaperType::kPolicy:
      return MojomWallpaperType::kPolicy;
    case ash::WallpaperType::kThirdParty:
      return MojomWallpaperType::kThirdParty;
    case ash::WallpaperType::kDevice:
      return MojomWallpaperType::kDevice;
    case ash::WallpaperType::kOneShot:
      return MojomWallpaperType::kOneShot;
    case ash::WallpaperType::kCount:
      NOTREACHED();
      return MojomWallpaperType::kDefault;
  }
}

bool EnumTraits<MojomWallpaperType, ash::WallpaperType>::FromMojom(
    MojomWallpaperType input,
    ash::WallpaperType* output) {
  switch (input) {
    case MojomWallpaperType::kDaily:
      *output = ash::WallpaperType::kDaily;
      return true;
    case MojomWallpaperType::kCustomized:
      *output = ash::WallpaperType::kCustomized;
      return true;
    case MojomWallpaperType::kDefault:
      *output = ash::WallpaperType::kDefault;
      return true;
    case MojomWallpaperType::kOnline:
      *output = ash::WallpaperType::kOnline;
      return true;
    case MojomWallpaperType::kPolicy:
      *output = ash::WallpaperType::kPolicy;
      return true;
    case MojomWallpaperType::kThirdParty:
      *output = ash::WallpaperType::kThirdParty;
      return true;
    case MojomWallpaperType::kDevice:
      *output = ash::WallpaperType::kDevice;
      return true;
    case MojomWallpaperType::kOneShot:
      *output = ash::WallpaperType::kOneShot;
      return true;
  }
  NOTREACHED();
  return false;
}

MojomOnlineImageType
EnumTraits<MojomOnlineImageType, ::backdrop::Image::ImageType>::ToMojom(
    ::backdrop::Image::ImageType input) {
  switch (input) {
    case ::backdrop::Image::IMAGE_TYPE_UNKNOWN:
      return MojomOnlineImageType::kUnknown;
    case ::backdrop::Image::IMAGE_TYPE_LIGHT_MODE:
      return MojomOnlineImageType::kLight;
    case ::backdrop::Image::IMAGE_TYPE_DARK_MODE:
      return MojomOnlineImageType::kDark;
  }
}

bool EnumTraits<MojomOnlineImageType, ::backdrop::Image::ImageType>::FromMojom(
    MojomOnlineImageType input,
    ::backdrop::Image::ImageType* output) {
  switch (input) {
    case MojomOnlineImageType::kUnknown:
      *output = ::backdrop::Image::IMAGE_TYPE_UNKNOWN;
      return true;
    case MojomOnlineImageType::kLight:
      *output = ::backdrop::Image::IMAGE_TYPE_LIGHT_MODE;
      return true;
    case MojomOnlineImageType::kDark:
      *output = ::backdrop::Image::IMAGE_TYPE_DARK_MODE;
      return true;
  }
  NOTREACHED();
  return false;
}

const std::string&
StructTraits<ash::personalization_app::mojom::WallpaperCollectionDataView,
             backdrop::Collection>::id(const backdrop::Collection& collection) {
  return collection.collection_id();
}

const std::string& StructTraits<
    ash::personalization_app::mojom::WallpaperCollectionDataView,
    backdrop::Collection>::name(const backdrop::Collection& collection) {
  return collection.collection_name();
}

absl::optional<GURL> StructTraits<
    ash::personalization_app::mojom::WallpaperCollectionDataView,
    backdrop::Collection>::preview(const backdrop::Collection& collection) {
  return GURL(collection.preview(0).image_url());
}

// Default to false as we don't ever need to convert back to
// |backdrop::Collection|
bool StructTraits<ash::personalization_app::mojom::WallpaperCollectionDataView,
                  backdrop::Collection>::
    Read(ash::personalization_app::mojom::WallpaperCollectionDataView data,
         backdrop::Collection* out) {
  return false;
}

bool StructTraits<ash::personalization_app::mojom::WallpaperCollectionDataView,
                  backdrop::Collection>::IsNull(const backdrop::Collection&
                                                    collection) {
  return !(collection.has_collection_id() && collection.has_collection_name() &&
           collection.preview_size() > 0);
}

GURL StructTraits<ash::personalization_app::mojom::WallpaperImageDataView,
                  backdrop::Image>::url(const backdrop::Image& image) {
  return GURL(image.image_url());
}

std::vector<std::string>
StructTraits<ash::personalization_app::mojom::WallpaperImageDataView,
             backdrop::Image>::attribution(const backdrop::Image& image) {
  std::vector<std::string> attribution;
  for (const auto& attr : image.attribution())
    attribution.push_back(attr.text());
  return attribution;
}

uint64_t StructTraits<ash::personalization_app::mojom::WallpaperImageDataView,
                      backdrop::Image>::asset_id(const backdrop::Image& image) {
  return image.asset_id();
}

uint64_t StructTraits<ash::personalization_app::mojom::WallpaperImageDataView,
                      backdrop::Image>::unit_id(const backdrop::Image& image) {
  return image.unit_id();
}

::backdrop::Image::ImageType
StructTraits<ash::personalization_app::mojom::WallpaperImageDataView,
             backdrop::Image>::type(const backdrop::Image& image) {
  return image.has_image_type() ? image.image_type()
                                : backdrop::Image::IMAGE_TYPE_UNKNOWN;
}

// Default to false as we don't ever need to convert back to
// |backdrop::Image|
bool StructTraits<ash::personalization_app::mojom::WallpaperImageDataView,
                  backdrop::Image>::
    Read(ash::personalization_app::mojom::WallpaperImageDataView data,
         backdrop::Image* out) {
  return false;
}

bool StructTraits<ash::personalization_app::mojom::WallpaperImageDataView,
                  backdrop::Image>::IsNull(const backdrop::Image& image) {
  if (!image.has_image_url() || !image.has_asset_id())
    return true;
  GURL image_url(image.image_url());
  if (!image_url.is_valid())
    return true;
  return false;
}

}  // namespace mojo
