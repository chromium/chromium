// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_TEST_HELPER_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_TEST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "components/ukm/test_ukm_recorder.h"

// Helper class for lookalike browser tests.
class LookalikeTestHelper {
 public:
  // Helper methods for interstitial and safety tips lookalike tests.
  // These allow the tests to use test data instead of prod, such as test top
  // domain lists.
  static void SetUpLookalikeTestParams();
  static void TearDownLookalikeTestParams();

  explicit LookalikeTestHelper(ukm::TestUkmRecorder* ukm_recorder);

  // Asserts that the safety tips UKM has `expected_event_count` entries.
  void CheckSafetyTipUkmCount(size_t expected_event_count) const;
  // Asserts that the interstitial UKM has `expected_event_count` entries.
  void CheckInterstitialUkmCount(size_t expected_event_count) const;

  // Asserts that no safety tip or interstitial UKMs were recorded.
  void CheckNoLookalikeUkm() const;

 private:
  raw_ptr<ukm::TestUkmRecorder> ukm_recorder_;
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_TEST_HELPER_H_
