// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/target_color_params.h"

#include <cmath>
#include <sstream>

#include "base/hash/hash.h"

namespace cc {

size_t TargetColorParams::GetHash() const {
  size_t hash = color_space.GetHash();
  if (hdr_headroom.has_value()) {
    const float f = hdr_headroom.value();
    const uint32_t* i = reinterpret_cast<const uint32_t*>(&f);
    hash = base::HashInts(hash, *i);
  }
  return hash;
}

std::string TargetColorParams::ToString() const {
  std::ostringstream str;
  str << "color_space: " << color_space.ToString();
  str << "hdr_headroom: ";
  if (hdr_headroom.has_value()) {
    str << hdr_headroom.value();
  } else {
    str << "(deferred)";
  }
  return str.str();
}

}  // namespace cc
