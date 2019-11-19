// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_field_trial_list_resetter.h"

#include "base/metrics/field_trial.h"

namespace base {
namespace test {

ScopedFieldTrialListResetter::ScopedFieldTrialListResetter()
    : original_field_trial_list_(
          base::FieldTrialList::BackupInstanceForTesting()) {}

ScopedFieldTrialListResetter::~ScopedFieldTrialListResetter() {
  base::FieldTrialList::RestoreInstanceForTesting(original_field_trial_list_);
}

}  // namespace test
}  // namespace base
