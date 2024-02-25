// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_resource_getter.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/nearby_sharing/internal/nearby_share_resource_utils.h"
#else  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/nearby_sharing/common/nearby_share_resource_utils.h"
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

#include "ui/base/l10n/l10n_util.h"

NearbyShareResourceGetter::NearbyShareResourceGetter() = default;

// static
NearbyShareResourceGetter* NearbyShareResourceGetter::GetInstance() {
  static base::NoDestructor<NearbyShareResourceGetter> instance;
  return instance.get();
}

std::u16string NearbyShareResourceGetter::GetFeatureName() {
  // Caller ensures feature flag is enabled.
  CHECK(features::IsNameEnabled());

  return GetNearbyShareFeatureName();
}

std::u16string NearbyShareResourceGetter::GetStringWithFeatureName(
    int message_id) {
  // Caller ensures feature flag is enabled.
  CHECK(features::IsNameEnabled());

  // Replace the placeholder at index 0 of the placeholder list with the feature
  // name.
  return l10n_util::GetStringFUTF16(message_id, GetNearbyShareFeatureName());
}
