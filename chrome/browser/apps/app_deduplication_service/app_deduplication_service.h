// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace apps {

class AppDeduplicationService : public KeyedService {
 public:
  explicit AppDeduplicationService(Profile* profile);
  ~AppDeduplicationService() override;
  AppDeduplicationService(const AppDeduplicationService&) = delete;
  AppDeduplicationService& operator=(const AppDeduplicationService&) = delete;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_H_
