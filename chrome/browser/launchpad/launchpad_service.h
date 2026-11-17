// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_
#define CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace launchpad {

class LaunchpadService : public KeyedService {
 public:
  LaunchpadService();

  LaunchpadService(const LaunchpadService&) = delete;
  LaunchpadService& operator=(const LaunchpadService&) = delete;
  ~LaunchpadService() override;
};

}  // namespace launchpad

#endif  // CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_
