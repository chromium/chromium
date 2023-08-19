// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/raster_capabilities.h"

namespace cc {

RasterCapabilities::RasterCapabilities() = default;

RasterCapabilities::RasterCapabilities(const RasterCapabilities& other) =
    default;

RasterCapabilities& RasterCapabilities::operator=(
    const RasterCapabilities& other) = default;

RasterCapabilities::~RasterCapabilities() = default;

}  // namespace cc
