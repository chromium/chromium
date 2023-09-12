// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"

namespace tpcd::experiment {

class EligibilityService : public KeyedService {
 public:
  explicit EligibilityService(Profile* profile);
  EligibilityService(const EligibilityService&) = delete;
  EligibilityService& operator=(const EligibilityService&) = delete;
  ~EligibilityService() override;

  static EligibilityService* Get(Profile* profile);

 private:
  // So EligibilityServiceFactory::BuildServiceInstanceFor can call the
  // constructor.
  friend class EligibilityServiceFactory;

  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;
};

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_
