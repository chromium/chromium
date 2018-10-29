// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_IO_DATA_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_IO_DATA_H_

#include <memory>

#include "base/memory/ref_counted.h"

class PrefService;

namespace base {
class SingleThreadTaskRunner;
}

namespace data_reduction_proxy {
class DataReductionProxyIOData;
}


// Constructs DataReductionProxyIOData suitable for use by ProfileImpl and
// ProfileImplIOData.
std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
CreateDataReductionProxyChromeIOData(
    PrefService* prefs,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread_runner);

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_IO_DATA_H_
