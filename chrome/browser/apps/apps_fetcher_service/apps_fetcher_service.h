// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_SERVICE_H_
#define CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace apps {

class AppsFetcherService : public KeyedService {
 public:
  explicit AppsFetcherService(Profile* profile);
  ~AppsFetcherService() override;
  AppsFetcherService(const AppsFetcherService&) = delete;
  AppsFetcherService& operator=(const AppsFetcherService&) = delete;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_SERVICE_H_
