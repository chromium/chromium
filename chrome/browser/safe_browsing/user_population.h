// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_USER_POPULATION_H_
#define CHROME_BROWSER_SAFE_BROWSING_USER_POPULATION_H_

#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/core/proto/csd.pb.h"

namespace safe_browsing {

// Creates a ChromeUserPopulation proto for the given |profile|.
ChromeUserPopulation GetUserPopulation(Profile* profile);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_USER_POPULATION_H_
