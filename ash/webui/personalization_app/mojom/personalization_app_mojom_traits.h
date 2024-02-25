// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_
#define ASH_WEBUI_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/default_user_image.h"
#include "ash/public/cpp/personalization_app/user_display_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/style/color_palette_controller.h"
#include "ash/style/mojom/color_scheme.mojom-shared.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "url/gurl.h"

namespace mojo {

template <>
struct EnumTraits<ash::personalization_app::mojom::WallpaperLayout,
                  ash::WallpaperLayout> {
  using MojomWallpaperLayout =
      ::ash::personalization_app::mojom::WallpaperLayout;
  static MojomWallpaperLayout ToMojom(ash::WallpaperLayout input);
  static bool FromMojom(MojomWallpaperLayout input,
                        ash::WallpaperLayout* output);
};

template <>
struct EnumTraits<ash::personalization_app::mojom::WallpaperType,
                  ash::WallpaperType> {
  using MojomWallpaperType = ::ash::personalization_app::mojom::WallpaperType;
  static MojomWallpaperType ToMojom(ash::WallpaperType input);
  static bool FromMojom(MojomWallpaperType input, ash::WallpaperType* output);
};

template <>
struct EnumTraits<ash::personalization_app::mojom::OnlineImageType,
                  ::backdrop::Image::ImageType> {
  using MojomOnlineImageType =
      ::ash::personalization_app::mojom::OnlineImageType;
  static MojomOnlineImageType ToMojom(::backdrop::Image::ImageType input);
  static bool FromMojom(MojomOnlineImageType input,
                        ::backdrop::Image::ImageType* output);
};

template <>
struct StructTraits<
    ash::personalization_app::mojom::WallpaperCollectionDataView,
    backdrop::Collection> {
  static const std::string& id(const backdrop::Collection& collection);
  static const std::string& name(const backdrop::Collection& collection);
  static const std::string& description_content(
      const backdrop::Collection& collection);
  static std::vector<GURL> previews(const backdrop::Collection& collection);

  static bool Read(
      ash::personalization_app::mojom::WallpaperCollectionDataView data,
      backdrop::Collection* out);
  static bool IsNull(const backdrop::Collection& collection);
};

template <>
struct StructTraits<ash::personalization_app::mojom::WallpaperImageDataView,
                    backdrop::Image> {
  static GURL url(const backdrop::Image& image);
  static std::vector<std::string> attribution(const backdrop::Image& image);
  static uint64_t asset_id(const backdrop::Image& image);
  static uint64_t unit_id(const backdrop::Image& image);
  static ::backdrop::Image::ImageType type(const backdrop::Image& image);

  static bool Read(ash::personalization_app::mojom::WallpaperImageDataView data,
                   backdrop::Image* out);
  static bool IsNull(const backdrop::Image& image);
};

template <>
struct StructTraits<ash::personalization_app::mojom::UserInfoDataView,
                    ash::personalization_app::UserDisplayInfo> {
  static const std::string& email(
      const ash::personalization_app::UserDisplayInfo& user_display_info);
  static const std::string& name(
      const ash::personalization_app::UserDisplayInfo& user_display_info);
  static bool Read(ash::personalization_app::mojom::UserInfoDataView data,
                   ash::personalization_app::UserDisplayInfo* out);
};

template <>
struct StructTraits<
    ash::personalization_app::mojom::DeprecatedSourceInfoDataView,
    ash::default_user_image::DeprecatedSourceInfo> {
  static const std::u16string& author(
      const ash::default_user_image::DeprecatedSourceInfo&
          deprecated_source_info);
  static const GURL& website(
      const ash::default_user_image::DeprecatedSourceInfo&
          deprecated_source_info);
  static bool Read(
      ash::personalization_app::mojom::DeprecatedSourceInfoDataView data,
      ash::default_user_image::DeprecatedSourceInfo* out);
};

template <>
struct StructTraits<ash::personalization_app::mojom::DefaultUserImageDataView,
                    ash::default_user_image::DefaultUserImage> {
  static int index(
      const ash::default_user_image::DefaultUserImage& default_user_image);
  static const std::u16string& title(
      const ash::default_user_image::DefaultUserImage& default_user_image);
  static const GURL& url(
      const ash::default_user_image::DefaultUserImage& default_user_image);
  static const std::optional<ash::default_user_image::DeprecatedSourceInfo>&
  source_info(
      const ash::default_user_image::DefaultUserImage& default_user_image);
  static bool Read(
      ash::personalization_app::mojom::DefaultUserImageDataView data,
      ash::default_user_image::DefaultUserImage* out);
};

template <>
struct EnumTraits<ash::personalization_app::mojom::TemperatureUnit,
                  ash::AmbientModeTemperatureUnit> {
  using MojomTemperatureUnit =
      ::ash::personalization_app::mojom::TemperatureUnit;
  static MojomTemperatureUnit ToMojom(ash::AmbientModeTemperatureUnit input);
  static bool FromMojom(MojomTemperatureUnit input,
                        ash::AmbientModeTemperatureUnit* output);
};

template <>
struct EnumTraits<ash::personalization_app::mojom::AmbientUiVisibility,
                  ash::AmbientUiVisibility> {
  using MojomAmbientUiVisibility =
      ::ash::personalization_app::mojom::AmbientUiVisibility;
  static MojomAmbientUiVisibility ToMojom(ash::AmbientUiVisibility input);
  static bool FromMojom(MojomAmbientUiVisibility input,
                        ash::AmbientUiVisibility* output);
};

template <>
struct StructTraits<ash::personalization_app::mojom::SampleColorSchemeDataView,
                    ash::SampleColorScheme> {
  static ash::style::mojom::ColorScheme scheme(
      const ash::SampleColorScheme& sample_color_scheme);
  static SkColor primary(const ash::SampleColorScheme& sample_color_scheme);
  static SkColor secondary(const ash::SampleColorScheme& sample_color_scheme);
  static SkColor tertiary(const ash::SampleColorScheme& sample_color_scheme);
  static bool Read(
      ash::personalization_app::mojom::SampleColorSchemeDataView data,
      ash::SampleColorScheme* out);
};

}  // namespace mojo

#endif  // ASH_WEBUI_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_
