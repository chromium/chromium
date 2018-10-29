// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_CHROME_NAVIGATION_DATA_H_
#define CHROME_BROWSER_LOADER_CHROME_NAVIGATION_DATA_H_

#include <memory>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "content/public/browser/navigation_data.h"

namespace net {
class URLRequest;
}

class ChromeNavigationData : public content::NavigationData,
                             public base::SupportsUserData::Data {
 public:
  ChromeNavigationData();
  ~ChromeNavigationData() override;

  // Creates a new ChromeNavigationData that is a deep copy of the original. Any
  // changes to the original after the clone is created will not be reflected in
  // the clone.
  // |data_reduction_proxy_data_| is deep copied.
  std::unique_ptr<content::NavigationData> Clone() const override;

  // Takes ownership of |data_reduction_proxy_data|.
  void SetDataReductionProxyData(
      std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
          data_reduction_proxy_data) {
    data_reduction_proxy_data_ = std::move(data_reduction_proxy_data);
  }

  data_reduction_proxy::DataReductionProxyData* GetDataReductionProxyData()
      const {
    return data_reduction_proxy_data_.get();
  }

  static ChromeNavigationData* GetDataAndCreateIfNecessary(
      net::URLRequest* request);

 private:
  // Manages the lifetime of optional DataReductionProxy information.
  std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
      data_reduction_proxy_data_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNavigationData);
};

#endif  // CHROME_BROWSER_LOADER_CHROME_NAVIGATION_DATA_H_
