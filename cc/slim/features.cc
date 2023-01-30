// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/features.h"

#include "base/feature_list.h"

namespace features {

BASE_FEATURE(kSlimCompositor,
             "SlimCompositor",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSlimCompositorEnabled() {
  return base::FeatureList::IsEnabled(kSlimCompositor);
}

}  // namespace features
