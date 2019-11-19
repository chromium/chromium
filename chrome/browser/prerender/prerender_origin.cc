// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_origin.h"

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "chrome/browser/prerender/prerender_manager.h"

namespace prerender {

namespace {

const char* kOriginNames[] = {
    "[Deprecated] Link Rel Prerender (original)",
    "[Deprecated] Omnibox (original)",
    "GWS Prerender",
    "[Deprecated] Omnibox (conservative)",
    "[Deprecated] Omnibox (exact)",
    "Omnibox",
    "None",
    "Link Rel Prerender (same domain)",
    "Link Rel Prerender (cross domain)",
    "[Deprecated] Local Predictor",
    "External Request",
    "[Deprecated] Instant",
    "[Deprecated] Link Rel Next",
    "External Request Forced Cellular",
    "[Deprecated] Offline",
    "Navigation Predictor",
    "Max",
};
static_assert(base::size(kOriginNames) == ORIGIN_MAX + 1,
              "prerender origin name count mismatch");

}  // namespace

const char* NameFromOrigin(Origin origin) {
  DCHECK(static_cast<int>(origin) >= 0 &&
         origin <= ORIGIN_MAX);
  return kOriginNames[origin];
}

}  // namespace prerender
