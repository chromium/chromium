// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_EXPORT_HELPER_H_
#define CHROME_BROWSER_NET_NET_EXPORT_HELPER_H_

#include "base/values.h"
#include "build/build_config.h"

class Profile;

namespace chrome_browser_net {

// Methods for getting Value summaries of net log polled data that need to be
// retrieved on the UI thread. All functions are expected to run on the UI
// thread. GetSessionNetworkStats() may return null if the info does not exist;
// others will always return a Value (possibly empty).

base::Value::Dict GetPrerenderInfo(Profile* profile);
base::Value::List GetExtensionInfo(Profile* profile);
#if BUILDFLAG(IS_WIN)
base::Value::Dict GetWindowsServiceProviders();
#endif

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_NET_EXPORT_HELPER_H_
