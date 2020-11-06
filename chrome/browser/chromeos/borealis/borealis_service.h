// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace borealis {

class BorealisFeatures;

class BorealisAppLauncher;

// A common location for all the interdependant components of borealis.
class BorealisService : public KeyedService {
 public:
  // Helper method to get the service instance for the given profile.
  static BorealisService* GetForProfile(Profile* profile);

  ~BorealisService() override = default;

  virtual BorealisFeatures& Features() = 0;

  virtual BorealisAppLauncher& AppLauncher() = 0;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_SERVICE_H_
