// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MALL_MALL_URL_H_
#define CHROME_BROWSER_ASH_MALL_MALL_URL_H_

class GURL;

namespace apps {
struct DeviceInfo;
}

namespace ash {

// Returns a URL to launch the mall app. Includes the `info` as additional
// context.
GURL GetMallLaunchUrl(const apps::DeviceInfo& info);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MALL_MALL_URL_H_
