// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_USER_POPULATION_HELPER_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_USER_POPULATION_HELPER_H_

#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace safe_browsing {

// A convenience function that creates a ChromeUserPopulation proto for the
// given |profile|.
ChromeUserPopulation GetUserPopulationForProfile(Profile* profile);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NoCachedPopulationReason {
  kUnknown = 0,
  kStartup = 1,
  kChangeMbbPref = 2,
  kChangeSbPref = 3,
  kMaxValue = kChangeSbPref,
};

// A cache of the ChromeUserPopulation is used to validate that we are
// consistently populating the same values into Safe Browsing pings. This should
// be cleared whenever we expect the correct value of the ChromeUserPopulation
// to be cleared. See crbug/1208532.
void ClearCachedUserPopulation(Profile* profile,
                               NoCachedPopulationReason reason);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_USER_POPULATION_HELPER_H_
