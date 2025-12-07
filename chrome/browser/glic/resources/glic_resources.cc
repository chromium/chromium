// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/resources/glic_resources.h"

#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/common/chrome_features.h"

namespace glic {

int GetResourceID(int id) {
  if (!base::FeatureList::IsEnabled(features::kGlicAssetsV2)) {
    return id;
  }

  // If kGlicAssetsV2 is enabled, look up the corresponding new resource.
  switch (id) {
    case IDR_GLIC_BUTTON_VECTOR_ICON:
      return IDR_GLIC_BUTTON_VECTOR_ICON_V2;
    case IDR_GLIC_STATUS_ICON:
      return IDR_GLIC_STATUS_ICON_V2;
    case IDR_GLIC_STATUS_ICON_DARK:
      return IDR_GLIC_STATUS_ICON_DARK_V2;
    case IDR_GLIC_STATUS_ICON_LIGHT:
      return IDR_GLIC_STATUS_ICON_LIGHT_V2;
    case IDR_GLIC_PROFILE_LOGO:
      return IDR_GLIC_PROFILE_LOGO_V2;
    case IDR_GLIC_PROFILE_BANNER_BOTTOM_LEFT:
      return IDR_GLIC_PROFILE_BANNER_BOTTOM_LEFT_V2;
    case IDR_GLIC_PROFILE_BANNER_TOP_RIGHT:
      return IDR_GLIC_PROFILE_BANNER_TOP_RIGHT_V2;
    case IDR_GLIC_PROFILE_BANNER_BOTTOM_LEFT_LIGHT:
      return IDR_GLIC_PROFILE_BANNER_BOTTOM_LEFT_LIGHT_V2;
    case IDR_GLIC_PROFILE_BANNER_TOP_RIGHT_LIGHT:
      return IDR_GLIC_PROFILE_BANNER_TOP_RIGHT_LIGHT_V2;

    default:
      return id;
  }
}

}  // namespace glic
