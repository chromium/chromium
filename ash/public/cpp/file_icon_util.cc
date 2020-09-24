// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/file_icon_util.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// Hex color: #796EEE
constexpr SkColor kFiletypeGsiteColor = SkColorSetRGB(121, 110, 238);

// Hex color: #FF7537
constexpr SkColor kFiletypePptColor = SkColorSetRGB(255, 117, 55);

// Hex color: #796EEE
constexpr SkColor kFiletypeSitesColor = SkColorSetRGB(121, 110, 238);

constexpr SkColor kWhiteBackgroundColor = SkColorSetRGB(255, 255, 255);

constexpr int kIconDipSize = 20;

}  // namespace

namespace ash {
namespace internal {

IconType GetIconTypeForPath(const base::FilePath& filepath) {
  static const base::NoDestructor<base::flat_map<std::string, IconType>>
      // Changes to this map should be reflected in
      // ui/file_manager/file_manager/common/js/file_type.js.
      extension_to_icon({
          // Image
          {".JPEG", IconType::kImage},
          {".JPG", IconType::kImage},
          {".BMP", IconType::kImage},
          {".GIF", IconType::kImage},
          {".ICO", IconType::kImage},
          {".PNG", IconType::kImage},
          {".WEBP", IconType::kImage},
          {".TIFF", IconType::kImage},
          {".TIF", IconType::kImage},
          {".SVG", IconType::kImage},

          // Raw
          {".ARW", IconType::kImage},
          {".CR2", IconType::kImage},
          {".DNG", IconType::kImage},
          {".NEF", IconType::kImage},
          {".NRW", IconType::kImage},
          {".ORF", IconType::kImage},
          {".RAF", IconType::kImage},
          {".RW2", IconType::kImage},

          // Video
          {".3GP", IconType::kVideo},
          {".3GPP", IconType::kVideo},
          {".AVI", IconType::kVideo},
          {".MOV", IconType::kVideo},
          {".MKV", IconType::kVideo},
          {".MP4", IconType::kVideo},
          {".M4V", IconType::kVideo},
          {".MPG", IconType::kVideo},
          {".MPEG", IconType::kVideo},
          {".MPG4", IconType::kVideo},
          {".MPEG4", IconType::kVideo},
          {".OGM", IconType::kVideo},
          {".OGV", IconType::kVideo},
          {".OGX", IconType::kVideo},
          {".WEBM", IconType::kVideo},

          // Audio
          {".AMR", IconType::kAudio},
          {".FLAC", IconType::kAudio},
          {".MP3", IconType::kAudio},
          {".M4A", IconType::kAudio},
          {".OGA", IconType::kAudio},
          {".OGG", IconType::kAudio},
          {".WAV", IconType::kAudio},

          // Text
          {".TXT", IconType::kGeneric},

          // Archive
          {".ZIP", IconType::kArchive},
          {".RAR", IconType::kArchive},
          {".TAR", IconType::kArchive},
          {".TAR.BZ2", IconType::kArchive},
          {".TBZ", IconType::kArchive},
          {".TBZ2", IconType::kArchive},
          {".TAR.GZ", IconType::kArchive},
          {".TGZ", IconType::kArchive},

          // Hosted doc
          {".GDOC", IconType::kGdoc},
          {".GSHEET", IconType::kGsheet},
          {".GSLIDES", IconType::kGslide},
          {".GDRAW", IconType::kGdraw},
          {".GTABLE", IconType::kGtable},
          {".GLINK", IconType::kGeneric},
          {".GFORM", IconType::kGform},
          {".GMAPS", IconType::kGmap},
          {".GSITE", IconType::kGsite},

          // Other
          {".PDF", IconType::kPdf},
          {".HTM", IconType::kGeneric},
          {".HTML", IconType::kGeneric},
          {".MHT", IconType::kGeneric},
          {".MHTM", IconType::kGeneric},
          {".MHTML", IconType::kGeneric},
          {".SHTML", IconType::kGeneric},
          {".XHT", IconType::kGeneric},
          {".XHTM", IconType::kGeneric},
          {".XHTML", IconType::kGeneric},
          {".DOC", IconType::kWord},
          {".DOCX", IconType::kWord},
          {".PPT", IconType::kPpt},
          {".PPTX", IconType::kPpt},
          {".XLS", IconType::kExcel},
          {".XLSX", IconType::kExcel},
          {".TINI", IconType::kTini},
      });

  const auto& icon_it =
      extension_to_icon->find(base::ToUpperASCII(filepath.Extension()));
  if (icon_it != extension_to_icon->end()) {
    return icon_it->second;
  } else {
    return IconType::kGeneric;
  }
}

IconType GetIconTypeFromString(const std::string& icon_type_string) {
  static const base::NoDestructor<std::map<std::string, IconType>>
      type_string_to_icon_type({{"archive", IconType::kArchive},
                                {"audio", IconType::kAudio},
                                {"chart", IconType::kChart},
                                {"excel", IconType::kExcel},
                                {"drive", IconType::kDrive},
                                {"folder", IconType::kFolder},
                                {"gdoc", IconType::kGdoc},
                                {"gdraw", IconType::kGdraw},
                                {"generic", IconType::kGeneric},
                                {"gform", IconType::kGform},
                                {"gmap", IconType::kGmap},
                                {"gsheet", IconType::kGsheet},
                                {"gsite", IconType::kGsite},
                                {"gslides", IconType::kGslide},
                                {"gtable", IconType::kGtable},
                                {"image", IconType::kImage},
                                {"linux", IconType::kLinux},
                                {"pdf", IconType::kPdf},
                                {"ppt", IconType::kPpt},
                                {"script", IconType::kScript},
                                {"shared", IconType::kFolderShared},
                                {"sites", IconType::kSites},
                                {"tini", IconType::kTini},
                                {"video", IconType::kVideo},
                                {"word", IconType::kWord}});

  const auto& icon_it = type_string_to_icon_type->find(icon_type_string);
  if (icon_it != type_string_to_icon_type->end())
    return icon_it->second;
  return IconType::kGeneric;
}

gfx::ImageSkia GetVectorIconFromIconType(IconType icon, bool is_chip_icon) {
  // Changes to this map should be reflected in
  // ui/file_manager/file_manager/common/js/file_type.js.
  static const base::NoDestructor<std::map<IconType, gfx::IconDescription>>
      icon_type_to_icon_description(
          {{IconType::kArchive,
            gfx::IconDescription(chromeos::kFiletypeArchiveIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::kAudio,
            gfx::IconDescription(chromeos::kFiletypeAudioIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::kChart,
            gfx::IconDescription(chromeos::kFiletypeChartIcon, kIconDipSize,
                                 gfx::kGoogleGreen500)},
           {IconType::kDrive,
            gfx::IconDescription(chromeos::kFiletypeTeamDriveIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::kExcel,
            gfx::IconDescription(chromeos::kFiletypeExcelIcon, kIconDipSize,
                                 gfx::kGoogleGreen500)},
           {IconType::kFolder,
            gfx::IconDescription(chromeos::kFiletypeFolderIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::kFolderShared,
            gfx::IconDescription(chromeos::kFiletypeSharedIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::kGdoc,
            gfx::IconDescription(chromeos::kFiletypeGdocIcon, kIconDipSize,
                                 gfx::kGoogleBlue500)},
           {IconType::kGdraw,
            gfx::IconDescription(chromeos::kFiletypeGdrawIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::kGeneric,
            gfx::IconDescription(chromeos::kFiletypeGenericIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::kGform,
            gfx::IconDescription(chromeos::kFiletypeGformIcon, kIconDipSize,
                                 gfx::kGoogleGreen500)},
           {IconType::kGmap,
            gfx::IconDescription(chromeos::kFiletypeGmapIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::kGsheet,
            gfx::IconDescription(chromeos::kFiletypeGsheetIcon, kIconDipSize,
                                 gfx::kGoogleGreen500)},
           {IconType::kGsite,
            gfx::IconDescription(chromeos::kFiletypeGsiteIcon, kIconDipSize,
                                 kFiletypeGsiteColor)},
           {IconType::kGslide,
            gfx::IconDescription(chromeos::kFiletypeGslidesIcon, kIconDipSize,
                                 gfx::kGoogleYellow500)},
           {IconType::kGtable,
            gfx::IconDescription(chromeos::kFiletypeGtableIcon, kIconDipSize,
                                 gfx::kGoogleGreen500)},
           {IconType::kImage,
            gfx::IconDescription(chromeos::kFiletypeImageIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::kLinux,
            gfx::IconDescription(chromeos::kFiletypeLinuxIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::kPdf,
            gfx::IconDescription(chromeos::kFiletypePdfIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::kPpt,
            gfx::IconDescription(chromeos::kFiletypePptIcon, kIconDipSize,
                                 kFiletypePptColor)},
           {IconType::kScript,
            gfx::IconDescription(chromeos::kFiletypeScriptIcon, kIconDipSize,
                                 gfx::kGoogleBlue500)},
           {IconType::kSites,
            gfx::IconDescription(chromeos::kFiletypeSitesIcon, kIconDipSize,
                                 kFiletypeSitesColor)},
           {IconType::kTini,
            gfx::IconDescription(chromeos::kFiletypeTiniIcon, kIconDipSize,
                                 gfx::kGoogleBlue500)},
           {IconType::kVideo,
            gfx::IconDescription(chromeos::kFiletypeVideoIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::kWord,
            gfx::IconDescription(chromeos::kFiletypeWordIcon, kIconDipSize,
                                 gfx::kGoogleBlue500)}});

  const auto& id_it = icon_type_to_icon_description->find(icon);
  DCHECK(id_it != icon_type_to_icon_description->end());

  // If it is a launcher chip icon, we need to draw 2 icons: a white circle
  // background icon (kFiletypeChipBackgroundIcon) and the icon of the file.
  if (is_chip_icon) {
    return gfx::ImageSkiaOperations::CreateSuperimposedImage(
        gfx::CreateVectorIcon(chromeos::kFiletypeChipBackgroundIcon,
                              kIconDipSize, kWhiteBackgroundColor),
        gfx::CreateVectorIcon(id_it->second));
  }
  return gfx::CreateVectorIcon(id_it->second);
}

}  // namespace internal

gfx::ImageSkia GetIconForPath(const base::FilePath& filepath) {
  return internal::GetVectorIconFromIconType(
      internal::GetIconTypeForPath(filepath));
}

gfx::ImageSkia GetChipIconForPath(const base::FilePath& filepath) {
  return internal::GetVectorIconFromIconType(
      internal::GetIconTypeForPath(filepath), /*is_chip_icon=*/true);
}

gfx::ImageSkia GetIconFromType(const std::string& icon_type) {
  return GetVectorIconFromIconType(internal::GetIconTypeFromString(icon_type));
}

}  // namespace ash
