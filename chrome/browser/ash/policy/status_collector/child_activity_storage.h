// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_CHILD_ACTIVITY_STORAGE_H_
#define CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_CHILD_ACTIVITY_STORAGE_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ash/policy/status_collector/activity_storage.h"

class PrefService;

namespace policy {

// Stores activity data in the pref service. It is only used during user session
// and for child users and retains the activity data up until a report is sent
// to the server.
class ChildActivityStorage : public ActivityStorage {
 public:
  // Forwards the arguments to ActivityStorage. Notice that |pref_service| is
  // expected to be a profile pref service.
  ChildActivityStorage(PrefService* pref_service,
                       const std::string& pref_name,
                       base::TimeDelta day_start_offset);
  ChildActivityStorage(const ChildActivityStorage&) = delete;
  ChildActivityStorage& operator=(const ChildActivityStorage&) = delete;
  ~ChildActivityStorage() override;

  // Adds an activity period.
  //
  // TODO(crbug.com/40569404): make a more appropriate signature once using
  // SimpleClockTest (as the |now| parameter is mostly for testing).
  void AddActivityPeriod(base::Time start, base::Time end, base::Time now);

  // Returns the list of stored activity periods.
  std::vector<enterprise_management::TimePeriod> GetStoredActivityPeriods();

 private:
  // Uses the PrefService to store child screen time.
  void StoreChildScreenTime(base::Time activity_day_start,
                            base::TimeDelta activity,
                            base::Time now);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_CHILD_ACTIVITY_STORAGE_H_
