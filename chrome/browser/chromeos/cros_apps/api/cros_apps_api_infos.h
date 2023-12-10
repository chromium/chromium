// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_INFOS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_INFOS_H_

#include <utility>
#include <vector>

#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature.mojom-forward.h"

class CrosAppsApiInfo;

std::vector<std::pair<blink::mojom::RuntimeFeature, CrosAppsApiInfo>>
CreateDefaultCrosAppsApiInfo();

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_INFOS_H_
