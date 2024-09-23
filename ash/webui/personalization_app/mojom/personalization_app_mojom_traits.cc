// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/mojom/personalization_app_mojom_traits.h"

#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/default_user_image.h"
#include "ash/public/cpp/personalization_app/user_display_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/style/mojom/color_scheme.mojom-shared.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

using MojomWallpaperLayout = ash::personalization_app::mojom::WallpaperLayout;
using MojomWallpaperType = ash::personalization_app::mojom::WallpaperType;
using MojomOnlineImageType = ash::personalization_app::mojom::OnlineImageType;
using MojomTemperatureUnit = ash::personalization_app::mojom::TemperatureUnit;
using MojomAmbientUiVisibility =
    ash::personalization_app::mojom::AmbientUiVisibility;

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
    case ash::WallpaperType::kDailyGooglePhotos:
      return MojomWallpaperType::kDailyGooglePhotos;
    case ash::WallpaperType::kOnceGooglePhotos:
      return MojomWallpaperType::kOnceGooglePhotos;
    case ash::WallpaperType::kOobe:
      return MojomWallpaperType::kOobe;
    case ash::WallpaperType::kSeaPen:
      return MojomWallpaperType::kSeaPen;
    case ash::WallpaperType::kCount:
      NOTREACHED();
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
    case MojomWallpaperType::kDailyGooglePhotos:
      *output = ash::WallpaperType::kDailyGooglePhotos;
      return true;
    case MojomWallpaperType::kOnceGooglePhotos:
      *output = ash::WallpaperType::kOnceGooglePhotos;
      return true;
    case MojomWallpaperType::kOobe:
      *output = ash::WallpaperType::kOobe;
      return true;
    case MojomWallpaperType::kSeaPen:
      *output = ash::WallpaperType::kSeaPen;
      return true;
  }
  NOTREACHED();
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
    case ::backdrop::Image::IMAGE_TYPE_MORNING_MODE:
      return MojomOnlineImageType::kMorning;
    case ::backdrop::Image::IMAGE_TYPE_LATE_AFTERNOON_MODE:
      return MojomOnlineImageType::kLateAfternoon;
    case ::backdrop::Image::IMAGE_TYPE_PREVIEW_MODE:
      return MojomOnlineImageType::kPreview;
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
    case MojomOnlineImageType::kMorning:
      *output = ::backdrop::Image::IMAGE_TYPE_MORNING_MODE;
      return true;
    case MojomOnlineImageType::kLateAfternoon:
      *output = ::backdrop::Image::IMAGE_TYPE_LATE_AFTERNOON_MODE;
      return true;
    case MojomOnlineImageType::kPreview:
      *output = ::backdrop::Image::IMAGE_TYPE_PREVIEW_MODE;
      return true;
  }
  NOTREACHED();
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

const std::string& StructTraits<
    ash::personalization_app::mojom::WallpaperCollectionDataView,
    backdrop::Collection>::description_content(const backdrop::Collection&
                                                   collection) {
  return collection.description_content();
}

std::vector<GURL> StructTraits<
    ash::personalization_app::mojom::WallpaperCollectionDataView,
    backdrop::Collection>::previews(const backdrop::Collection& collection) {
  std::vector<GURL> previews;
  for (const auto& image : collection.preview()) {
    previews.push_back(GURL(image.image_url()));
  }
  return previews;
}

// Default to false as we don't ever need to convert back to
// `backdrop::Collection`
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
// `Backdrop::Image`
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

const std::string&
StructTraits<ash::personalization_app::mojom::UserInfoDataView,
             ash::personalization_app::UserDisplayInfo>::
    email(const ash::personalization_app::UserDisplayInfo& user_display_info) {
  return user_display_info.email;
}

const std::string&
StructTraits<ash::personalization_app::mojom::UserInfoDataView,
             ash::personalization_app::UserDisplayInfo>::
    name(const ash::personalization_app::UserDisplayInfo& user_display_info) {
  return user_display_info.name;
}

bool StructTraits<ash::personalization_app::mojom::UserInfoDataView,
                  ash::personalization_app::UserDisplayInfo>::
    Read(ash::personalization_app::mojom::UserInfoDataView data,
         ash::personalization_app::UserDisplayInfo* out) {
  return data.ReadEmail(&out->email) && data.ReadName(&out->name);
}

const std::u16string&
StructTraits<ash::personalization_app::mojom::DeprecatedSourceInfoDataView,
             ash::default_user_image::DeprecatedSourceInfo>::
    author(const ash::default_user_image::DeprecatedSourceInfo&
               deprecated_source_info) {
  return deprecated_source_info.author;
}

const GURL&
StructTraits<ash::personalization_app::mojom::DeprecatedSourceInfoDataView,
             ash::default_user_image::DeprecatedSourceInfo>::
    website(const ash::default_user_image::DeprecatedSourceInfo&
                deprecated_source_info) {
  return deprecated_source_info.website;
}

bool StructTraits<ash::personalization_app::mojom::DeprecatedSourceInfoDataView,
                  ash::default_user_image::DeprecatedSourceInfo>::
    Read(ash::personalization_app::mojom::DeprecatedSourceInfoDataView data,
         ash::default_user_image::DeprecatedSourceInfo* out) {
  return data.ReadAuthor(&out->author) && data.ReadWebsite(&out->website);
}

int StructTraits<ash::personalization_app::mojom::DefaultUserImageDataView,
                 ash::default_user_image::DefaultUserImage>::
    index(const ash::default_user_image::DefaultUserImage& default_user_image) {
  return default_user_image.index;
}

const std::u16string&
StructTraits<ash::personalization_app::mojom::DefaultUserImageDataView,
             ash::default_user_image::DefaultUserImage>::
    title(const ash::default_user_image::DefaultUserImage& default_user_image) {
  return default_user_image.title;
}

const GURL&
StructTraits<ash::personalization_app::mojom::DefaultUserImageDataView,
             ash::default_user_image::DefaultUserImage>::
    url(const ash::default_user_image::DefaultUserImage& default_user_image) {
  return default_user_image.url;
}

const std::optional<ash::default_user_image::DeprecatedSourceInfo>&
StructTraits<ash::personalization_app::mojom::DefaultUserImageDataView,
             ash::default_user_image::DefaultUserImage>::
    source_info(
        const ash::default_user_image::DefaultUserImage& default_user_image) {
  return default_user_image.source_info;
}

bool StructTraits<ash::personalization_app::mojom::DefaultUserImageDataView,
                  ash::default_user_image::DefaultUserImage>::
    Read(ash::personalization_app::mojom::DefaultUserImageDataView data,
         ash::default_user_image::DefaultUserImage* out) {
  out->index = data.index();
  return data.ReadTitle(&out->title) && data.ReadUrl(&out->url) &&
         data.ReadSourceInfo(&out->source_info);
}

MojomTemperatureUnit
EnumTraits<MojomTemperatureUnit, ash::AmbientModeTemperatureUnit>::ToMojom(
    ash::AmbientModeTemperatureUnit input) {
  switch (input) {
    case ash::AmbientModeTemperatureUnit::kFahrenheit:
      return MojomTemperatureUnit::kFahrenheit;
    case ash::AmbientModeTemperatureUnit::kCelsius:
      return MojomTemperatureUnit::kCelsius;
  }
}

bool EnumTraits<MojomTemperatureUnit, ash::AmbientModeTemperatureUnit>::
    FromMojom(MojomTemperatureUnit input,
              ash::AmbientModeTemperatureUnit* output) {
  switch (input) {
    case MojomTemperatureUnit::kFahrenheit:
      *output = ash::AmbientModeTemperatureUnit::kFahrenheit;
      return true;
    case MojomTemperatureUnit::kCelsius:
      *output = ash::AmbientModeTemperatureUnit::kCelsius;
      return true;
  }
  NOTREACHED();
}

MojomAmbientUiVisibility
EnumTraits<MojomAmbientUiVisibility, ash::AmbientUiVisibility>::ToMojom(
    ash::AmbientUiVisibility input) {
  switch (input) {
    case ash::AmbientUiVisibility::kShouldShow:
      return MojomAmbientUiVisibility::kShouldShow;
    case ash::AmbientUiVisibility::kPreview:
      return MojomAmbientUiVisibility::kPreview;
    case ash::AmbientUiVisibility::kHidden:
      return MojomAmbientUiVisibility::kHidden;
    case ash::AmbientUiVisibility::kClosed:
      return MojomAmbientUiVisibility::kClosed;
  }
}

bool EnumTraits<MojomAmbientUiVisibility, ash::AmbientUiVisibility>::FromMojom(
    MojomAmbientUiVisibility input,
    ash::AmbientUiVisibility* output) {
  switch (input) {
    case MojomAmbientUiVisibility::kShouldShow:
      *output = ash::AmbientUiVisibility::kShouldShow;
      return true;
    case MojomAmbientUiVisibility::kPreview:
      *output = ash::AmbientUiVisibility::kPreview;
      return true;
    case MojomAmbientUiVisibility::kHidden:
      *output = ash::AmbientUiVisibility::kHidden;
      return true;
    case MojomAmbientUiVisibility::kClosed:
      *output = ash::AmbientUiVisibility::kClosed;
      return true;
  }
  NOTREACHED();
}

SkColor
StructTraits<ash::personalization_app::mojom::SampleColorSchemeDataView,
             ash::SampleColorScheme>::primary(const ash::SampleColorScheme&
                                                  sample_color_scheme) {
  return sample_color_scheme.primary;
}

SkColor
StructTraits<ash::personalization_app::mojom::SampleColorSchemeDataView,
             ash::SampleColorScheme>::secondary(const ash::SampleColorScheme&
                                                    sample_color_scheme) {
  return sample_color_scheme.secondary;
}

SkColor
StructTraits<ash::personalization_app::mojom::SampleColorSchemeDataView,
             ash::SampleColorScheme>::tertiary(const ash::SampleColorScheme&
                                                   sample_color_scheme) {
  return sample_color_scheme.tertiary;
}

ash::style::mojom::ColorScheme
StructTraits<ash::personalization_app::mojom::SampleColorSchemeDataView,
             ash::SampleColorScheme>::scheme(const ash::SampleColorScheme&
                                                 sample_color_scheme) {
  return sample_color_scheme.scheme;
}

bool StructTraits<ash::personalization_app::mojom::SampleColorSchemeDataView,
                  ash::SampleColorScheme>::
    Read(ash::personalization_app::mojom::SampleColorSchemeDataView data,
         ash::SampleColorScheme* out) {
  return data.ReadScheme(&out->scheme) && data.ReadPrimary(&out->primary) &&
         data.ReadSecondary(&out->secondary) &&
         data.ReadTertiary(&out->tertiary);
}
}  // namespace mojo
