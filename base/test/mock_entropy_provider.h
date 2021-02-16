// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_MOCK_ENTROPY_PROVIDER_H_
#define BASE_TEST_MOCK_ENTROPY_PROVIDER_H_

#include <stdint.h>

#include "base/metrics/field_trial.h"

namespace base {

class MockEntropyProvider : public base::FieldTrial::EntropyProvider {
 public:
  MockEntropyProvider();
  explicit MockEntropyProvider(double entropy_value);
  ~MockEntropyProvider() override;

  // base::FieldTrial::EntropyProvider:
  double GetEntropyForTrial(StringPiece trial_name,
                            uint32_t randomization_seed) const override;

 private:
  double entropy_value_;

  DISALLOW_COPY_AND_ASSIGN(MockEntropyProvider);
};

}  // namespace base

#endif  // BASE_TEST_MOCK_ENTROPY_PROVIDER_H_
