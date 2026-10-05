// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/glic_util.h"

#include "base/feature_list.h"
#include "extensions/common/extension_features.h"

namespace extensions {

bool IsApiGlicPrivateEnabled() {
  return base::FeatureList::IsEnabled(extensions_features::kApiGlicPrivate);
}

}  // namespace extensions
