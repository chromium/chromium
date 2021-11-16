// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_
#define ASH_WEBUI_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  static absl::optional<GURL> preview(const backdrop::Collection& collection);

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

}  // namespace mojo

#endif  // ASH_WEBUI_PERSONALIZATION_APP_MOJOM_PERSONALIZATION_APP_MOJOM_TRAITS_H_
