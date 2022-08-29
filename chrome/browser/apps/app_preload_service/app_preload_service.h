// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace apps {

class AppPreloadService : public KeyedService {
 public:
  explicit AppPreloadService(Profile* profile);
  AppPreloadService(const AppPreloadService&) = delete;
  AppPreloadService& operator=(const AppPreloadService&) = delete;
  ~AppPreloadService() override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_H_
