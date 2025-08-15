// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

class AimEligibilityServiceObserver : public base::CheckedObserver {
 public:
  virtual void OnAimEligibilityServiceChanged() = 0;

 protected:
  ~AimEligibilityServiceObserver() override = default;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_OBSERVER_H_
