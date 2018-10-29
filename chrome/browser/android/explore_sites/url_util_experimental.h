// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_URL_UTIL_EXPERIMENTAL_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_URL_UTIL_EXPERIMENTAL_H_

#include "url/gurl.h"

namespace explore_sites {

// Returns the base URL for the Explore Sites server.
GURL GetBasePrototypeURL();

// Returns the NTP JSON URL for the Explore Sites feature.
GURL GetNtpPrototypeURL();

// Returns the ESP catalog URL for the Explore Sites feature.
GURL GetCatalogPrototypeURL();

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_URL_UTIL_EXPERIMENTAL_H_
