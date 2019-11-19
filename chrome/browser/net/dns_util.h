// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_UTIL_H_
#define CHROME_BROWSER_NET_DNS_UTIL_H_

#include <string>

namespace chrome_browser_net {

// Returns true if the URI template is acceptable for sending requests. If so,
// the |server_method| is set to "GET" if the template contains a "dns" variable
// and to "POST" otherwise. Any "dns" variable may not be part of the hostname,
// and the expanded template must parse to a valid HTTPS URL.
bool IsValidDohTemplate(const std::string& server_template,
                        std::string* server_method);

// Returns true if there are any active machine level policies or if the machine
// is domain joined. This special logic is used to disable DoH by default for
// Desktop platforms (the enterprise policy field default_for_enterprise_users
// only applies to ChromeOS). We don't attempt enterprise detection on Android
// at this time.
bool ShouldDisableDohForManaged();

const char kDnsOverHttpsModeOff[] = "off";
const char kDnsOverHttpsModeAutomatic[] = "automatic";
const char kDnsOverHttpsModeSecure[] = "secure";

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_UTIL_H_
