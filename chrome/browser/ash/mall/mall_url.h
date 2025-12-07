// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MALL_MALL_URL_H_
#define CHROME_BROWSER_ASH_MALL_MALL_URL_H_

#include <string_view>

class GURL;

namespace apps {
struct DeviceInfo;
}

namespace ash {

// Returns a URL to launch the mall app. Includes the `info` as additional
// context, and sets the URL's `path`, if provided. If setting `path` would
// result in an invalid URL, it is ignored.
GURL GetMallLaunchUrl(const apps::DeviceInfo& info, std::string_view path = "");

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MALL_MALL_URL_H_
