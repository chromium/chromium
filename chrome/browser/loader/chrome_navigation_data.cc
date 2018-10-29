// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/chrome_navigation_data.h"

#include "base/memory/ptr_util.h"
#include "net/url_request/url_request.h"

const void* const kChromeNavigationDataUserDataKey =
    &kChromeNavigationDataUserDataKey;

ChromeNavigationData::ChromeNavigationData() {}

ChromeNavigationData::~ChromeNavigationData() {}

ChromeNavigationData* ChromeNavigationData::GetDataAndCreateIfNecessary(
    net::URLRequest* request) {
  if (!request)
    return nullptr;
  ChromeNavigationData* data = static_cast<ChromeNavigationData*>(
      request->GetUserData(kChromeNavigationDataUserDataKey));
  if (data)
    return data;
  data = new ChromeNavigationData();
  request->SetUserData(kChromeNavigationDataUserDataKey,
                       base::WrapUnique(data));
  return data;
}

std::unique_ptr<content::NavigationData> ChromeNavigationData::Clone() const {
  std::unique_ptr<ChromeNavigationData> copy(new ChromeNavigationData());
  if (data_reduction_proxy_data_) {
    copy->SetDataReductionProxyData(data_reduction_proxy_data_->DeepCopy());
  }
  return std::move(copy);
}
