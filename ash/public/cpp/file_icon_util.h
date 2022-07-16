// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FILE_ICON_UTIL_H_
#define ASH_PUBLIC_CPP_FILE_ICON_UTIL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/files/file_path.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

enum class IconType {
  kAudio,
  kArchive,
  kChart,
  kDrive,
  kExcel,
  kFolder,
  kFolderShared,
  kGdoc,
  kGdraw,
  kGeneric,
  kGform,
  kGmap,
  kGsheet,
  kGsite,
  kGslide,
  kGtable,
  kLinux,
  kImage,
  kPdf,
  kPpt,
  kScript,
  kSites,
  kTini,
  kVideo,
  kWord,
};

namespace internal {

ASH_PUBLIC_EXPORT IconType
GetIconTypeFromString(const std::string& icon_type_string);

ASH_PUBLIC_EXPORT IconType GetIconTypeForPath(const base::FilePath& filepath);

}  // namespace internal

// Returns the file type icon for the specified `filepath`. If `dark_background`
// is `true`, lighter foreground colors are used to ensure sufficient contrast.
ASH_PUBLIC_EXPORT gfx::ImageSkia GetIconForPath(const base::FilePath& file_path,
                                                bool dark_background);

// Returns the file type chip icon for the specified `filepath`.
ASH_PUBLIC_EXPORT gfx::ImageSkia GetChipIconForPath(
    const base::FilePath& filepath,
    bool dark_background);

// Returns the file type icon for the specified `icon_type`.
ASH_PUBLIC_EXPORT gfx::ImageSkia GetIconFromType(const std::string& icon_type,
                                                 bool dark_background);

// Returns the file type icon for the specified `icon_type`. If
// `dark_background` is `true`, lighter foreground colors are used to ensure
// sufficient contrast.
ASH_PUBLIC_EXPORT gfx::ImageSkia GetIconFromType(IconType icon_type,
                                                 bool dark_background);

// Returns the resolved color of the file type icon for the specified
// `filepath`. If `dark_background` is `true`, lighter foreground colors are
// used to ensure sufficient contrast.
ASH_PUBLIC_EXPORT SkColor GetIconColorForPath(const base::FilePath& filepath,
                                              bool dark_background);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FILE_ICON_UTIL_H_
