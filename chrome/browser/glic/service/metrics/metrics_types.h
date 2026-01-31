// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_METRICS_TYPES_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_METRICS_TYPES_H_

#include <string>

namespace glic {

enum class DaisyChainSource {
  kUnknown = 0,
  kGlicContents = 1,
  kTabContents = 2,
  kActorAddTab = 3,
  kNewTab = 4,
  kMaxValue = kNewTab,
};

std::string GetDaisyChainSourceString(DaisyChainSource source);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_METRICS_TYPES_H_
