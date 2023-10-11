// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/target_color_params.h"

#include <sstream>

#include "base/hash/hash.h"

namespace cc {

size_t TargetColorParams::GetHash() const {
  const uint32_t* hdr_max_luminance_relative_int =
      reinterpret_cast<const uint32_t*>(&hdr_max_luminance_relative);
  const uint32_t* sdr_max_luminance_nits_int =
      reinterpret_cast<const uint32_t*>(&sdr_max_luminance_nits);
  size_t hash = color_space.GetHash();
  hash = base::HashInts(hash, *hdr_max_luminance_relative_int);
  hash = base::HashInts(hash, *sdr_max_luminance_nits_int);
  return hash;
}

std::string TargetColorParams::ToString() const {
  std::ostringstream str;
  str << "color_space: " << color_space.ToString()
      << "sdr_max_luminance_nits: " << sdr_max_luminance_nits
      << "hdr_max_luminance_relative: " << hdr_max_luminance_relative;
  return str.str();
}

}  // namespace cc
