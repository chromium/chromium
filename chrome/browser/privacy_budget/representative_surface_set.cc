// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/representative_surface_set.h"

#include "base/strings/string_number_conversions.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

std::string RepresentativeSurfaceToString(const RepresentativeSurface& v) {
  return base::NumberToString(v.value().ToUkmMetricHash());
}

size_t RepresentativeSurfaceHash::operator()(
    const RepresentativeSurface& s) const {
  blink::IdentifiableSurfaceHash hasher;
  return hasher(s.value());
}
