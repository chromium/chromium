// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_H_
#define CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class FilePath;
}

class HeavyAdBlocklist;

// Keyed service that owns the heavy ad intervention blocklist.
class HeavyAdService : public KeyedService,
                       public blacklist::OptOutBlacklistDelegate {
 public:
  HeavyAdService();
  ~HeavyAdService() override;

  // Initializes the UI Service. |profile_path| is the path to user data on
  // disk.
  void Initialize(const base::FilePath& profile_path);

  // Initializes the blocklist with no backing store for incognito mode.
  void InitializeOffTheRecord();

  HeavyAdBlocklist* heavy_ad_blocklist() { return heavy_ad_blocklist_.get(); }

 private:
  // The blocklist used to control triggering of the heavy ad intervention.
  // Created during Initialize().
  std::unique_ptr<HeavyAdBlocklist> heavy_ad_blocklist_;

  DISALLOW_COPY_AND_ASSIGN(HeavyAdService);
};

#endif  // CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_H_
