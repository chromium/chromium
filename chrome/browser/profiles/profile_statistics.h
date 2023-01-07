// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_H_
#define CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class ProfileStatisticsAggregator;

// Instances of ProfileStatistics should be created directly. Use
// ProfileStatisticsFactory instead.
class ProfileStatistics : public KeyedService {
 public:
  // Profile Statistics --------------------------------------------------------

  // This function collects statistical information about |profile|, also
  // returns the information via |callback| if |callback| is not null.
  // Currently bookmarks, history, logins and autofill forms are counted. The
  // callback function will probably be called more than once, so binding
  // parameters with bind::Passed() is prohibited.
  void GatherStatistics(profiles::ProfileStatisticsCallback callback);

 private:
  friend class ProfileStatisticsFactory;

  explicit ProfileStatistics(Profile* profile);
  ~ProfileStatistics() override;
  void DeregisterAggregator();

  raw_ptr<Profile> profile_;
  std::unique_ptr<ProfileStatisticsAggregator> aggregator_;
  base::WeakPtrFactory<ProfileStatistics> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_H_
