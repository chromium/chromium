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
  SeaPenImage(std::string jpg_bytes,
              uint32_t id,
              std::string generative_prompt = "");

  SeaPenImage(SeaPenImage&& other);
  SeaPenImage& operator=(SeaPenImage&& other);

  SeaPenImage(const SeaPenImage&) = delete;
  SeaPenImage& operator=(const SeaPenImage&) = delete;

  ~SeaPenImage();

  // A bytes string of the image data encoded in jpg format.
  std::string jpg_bytes;

  // A unique identifier for this image. Set by the Manta API.
  uint32_t id;

  // The prompt used to generate the image. This is not always the same as the
  // user-provided prompt.
  std::string generative_prompt;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_SEA_PEN_IMAGE_H_
