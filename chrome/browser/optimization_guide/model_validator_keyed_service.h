// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace optimization_guide {

// Keyed service that validates models when enabled via command-line switch.
class ModelValidatorKeyedService : public KeyedService {
 public:
  explicit ModelValidatorKeyedService(Profile* profile);
  ~ModelValidatorKeyedService() override;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_H_
