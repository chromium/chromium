// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_SEA_PEN_IMAGE_H_
#define ASH_PUBLIC_CPP_WALLPAPER_SEA_PEN_IMAGE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// A simplified version of a Manta API response with only the relevant fields
// for SeaPen wallpaper.
// @see //components/manta
struct ASH_PUBLIC_EXPORT SeaPenImage {
  bool operator==(const SeaPenImage& other) const = default;

  // A bytes string of the image data encoded in jpg format.
  std::string jpg_bytes;

  // A unique identifier for this image. Set by the Manta API.
  uint32_t id;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_SEA_PEN_IMAGE_H_
