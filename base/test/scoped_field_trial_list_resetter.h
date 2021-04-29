// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_FIELD_TRIAL_LIST_RESETTER_H_
#define BASE_TEST_SCOPED_FIELD_TRIAL_LIST_RESETTER_H_

namespace base {

class FieldTrialList;

namespace test {

// DISCLAIMER: Please use ScopedFeatureList except for advanced cases where
// custom instantiation of FieldTrialList is required.
//
// ScopedFieldTrialListResetter resets the global FieldTrialList instance to
// null, and restores the original state when the class goes out of scope. This
// allows client code to initialize FieldTrialList instances in a custom
// fashion.
class ScopedFieldTrialListResetter final {
 public:
  ScopedFieldTrialListResetter();
  ScopedFieldTrialListResetter(const ScopedFieldTrialListResetter&) = delete;
  ScopedFieldTrialListResetter(ScopedFieldTrialListResetter&&) = delete;

  ~ScopedFieldTrialListResetter();

 private:
  base::FieldTrialList* const original_field_trial_list_;
};

}  // namespace test
}  // namespace base

#endif  // BASE_TEST_SCOPED_FIELD_TRIAL_LIST_RESETTER_H_
