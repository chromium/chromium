// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/metrics_types.h"

namespace glic {

std::string GetDaisyChainSourceString(DaisyChainSource source) {
  switch (source) {
    case DaisyChainSource::kGlicContents:
      return "GlicContents";
    case DaisyChainSource::kTabContents:
      return "TabContents";
    case DaisyChainSource::kActorAddTab:
      return "ActorAddTab";
    case DaisyChainSource::kNewTab:
      return "NewTab";
    default:
      return "Unknown";
  }
}

}  // namespace glic
