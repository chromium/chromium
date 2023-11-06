// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_resource_utils.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// Returns "Nearby Share".
std::u16string GetNearbyShareFeatureName() {
  return l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME);
}
