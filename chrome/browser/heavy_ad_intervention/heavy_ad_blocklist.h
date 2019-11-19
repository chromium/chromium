// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_BLOCKLIST_H_
#define CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_BLOCKLIST_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist.h"

namespace base {
class Clock;
}

namespace blacklist {
class OptOutBlacklistDelegate;
class OptOutStore;
}  // namespace blacklist

// The heavy ad intervention only supports one type for the blocklist.
enum class HeavyAdBlocklistType {
  kHeavyAdOnlyType = 0,
};

// A class that manages opt out blacklist parameters for the heavy ad
// intervention. The blocklist is used to allow at most 5 interventions per top
// frame origin per day. This prevents the intervention from being used as a
// cross-origin side channel.
class HeavyAdBlocklist : public blacklist::OptOutBlacklist {
 public:
  HeavyAdBlocklist(std::unique_ptr<blacklist::OptOutStore> opt_out_store,
                   base::Clock* clock,
                   blacklist::OptOutBlacklistDelegate* blacklist_delegate);
  ~HeavyAdBlocklist() override;

 protected:
  // OptOutBlacklist:
  bool ShouldUseSessionPolicy(base::TimeDelta* duration,
                              size_t* history,
                              int* threshold) const override;
  bool ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                 size_t* history,
                                 int* threshold) const override;
  bool ShouldUseHostPolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold,
                           size_t* max_hosts) const override;
  bool ShouldUseTypePolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold) const override;
  blacklist::BlacklistData::AllowedTypesAndVersions GetAllowedTypes()
      const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeavyAdBlocklist);
};

#endif  // CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_BLOCKLIST_H_
