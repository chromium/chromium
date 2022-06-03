// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_util.h"

#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"

bool IsLiteVideoAllowedForUser(Profile* profile) {
  if (profile->IsOffTheRecord())
    return false;

  if (!lite_video::features::IsLiteVideoEnabled())
    return false;

  // Check if they are a data saver user.
  return data_reduction_proxy::DataReductionProxySettings::
      IsDataSaverEnabledByUser(profile->IsOffTheRecord(), profile->GetPrefs());
}
