// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_EXPORT_HELPER_H_
#define CHROME_BROWSER_NET_NET_EXPORT_HELPER_H_

#include <memory>

namespace base {
class Value;
class DictionaryValue;
class ListValue;
}
class Profile;

namespace chrome_browser_net {

// Methods for getting Value summaries of net log polled data that need to be
// retrieved on the UI thread. All functions are expected to run on the UI
// thread. GetSessionNetworkStats() may return null if the info does not exist;
// others will always return a Value (possibly empty).

std::unique_ptr<base::DictionaryValue> GetPrerenderInfo(Profile* profile);
std::unique_ptr<base::ListValue> GetExtensionInfo(Profile* profile);
#if defined(OS_WIN)
std::unique_ptr<base::DictionaryValue> GetWindowsServiceProviders();
#endif

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_NET_EXPORT_HELPER_H_
