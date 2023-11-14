// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/sea_pen_image.h"

#include <string>
#include <utility>

#include "components/manta/proto/manta.pb.h"

namespace ash {

SeaPenImage::SeaPenImage(std::string jpg_bytes_in,
                         uint32_t id_in,
                         const std::string& query_in,
                         manta::proto::ImageResolution resolution_in)
    : jpg_bytes(std::move(jpg_bytes_in)),
      id(id_in),
      query(query_in),
      resolution(resolution_in) {}

SeaPenImage::SeaPenImage(SeaPenImage&&) = default;
SeaPenImage& SeaPenImage::operator=(SeaPenImage&&) = default;

SeaPenImage::~SeaPenImage() = default;

}  // namespace ash
