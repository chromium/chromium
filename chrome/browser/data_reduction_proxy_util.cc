// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_reduction_proxy_util.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "content/public/browser/resource_context.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#endif

bool IsDataReductionProxyResourceThrottleEnabledForUrl(
    content::ResourceContext* resource_context,
    const GURL& url) {
#if defined(OS_ANDROID)
  if (data_reduction_proxy::params::
          IsIncludedInOnDeviceSafeBrowsingFieldTrial()) {
    return false;
  }
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  return !io_data->IsOffTheRecord() &&
         io_data->data_reduction_proxy_io_data() &&
         io_data->data_reduction_proxy_io_data()->IsEnabled() &&
         !url.SchemeIsCryptographic();
#else
  return false;
#endif
}
