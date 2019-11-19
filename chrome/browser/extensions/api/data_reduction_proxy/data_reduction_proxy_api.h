// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_API_H_

#include <memory>

#include "extensions/browser/extension_function.h"

namespace data_reduction_proxy {
class DataUsageBucket;
}

namespace extensions {

class DataReductionProxyClearDataSavingsFunction : public ExtensionFunction {
 private:
  ~DataReductionProxyClearDataSavingsFunction() override {}

  DECLARE_EXTENSION_FUNCTION("dataReductionProxy.clearDataSavings",
                             DATAREDUCTIONPROXY_CLEARDATASAVINGS)

  ResponseAction Run() override;
};

class DataReductionProxyGetDataUsageFunction : public ExtensionFunction {
 private:
  ~DataReductionProxyGetDataUsageFunction() override {}

  DECLARE_EXTENSION_FUNCTION("dataReductionProxy.getDataUsage",
                             DATAREDUCTIONPROXY_GETDATAUSAGE)

  ResponseAction Run() override;

  void ReplyWithDataUsage(
      std::unique_ptr<std::vector<data_reduction_proxy::DataUsageBucket>>
          data_usage);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_API_H_
