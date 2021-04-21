// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/omaha_attributes_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Extension ids used during testing.
constexpr char kTestExtensionId[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";

}  // namespace

// Test suite to test Omaha attribute handler.
using OmahaAttributesHandlerUnitTest = ExtensionServiceTestBase;

TEST_F(OmahaAttributesHandlerUnitTest, LogPolicyViolationUWSMetrics) {
  base::HistogramTester histograms;
  base::Value attributes(base::Value::Type::DICTIONARY);
  attributes.SetKey("_potentially_uws", base::Value(true));
  attributes.SetKey("_policy_violation", base::Value(true));
  InitializeEmptyExtensionService();

  service()->PerformActionBasedOnOmahaAttributes(kTestExtensionId, attributes);

  histograms.ExpectBucketCount(
      "Extensions.ExtensionAddDisabledRemotelyReason",
      /* sample */ ExtensionUpdateCheckDataKey::kPotentiallyUWS,
      /* expected_count */ 1);
  histograms.ExpectBucketCount(
      "Extensions.ExtensionAddDisabledRemotelyReason",
      /* sample */ ExtensionUpdateCheckDataKey::kPolicyViolation,
      /* expected_count */ 1);
}

}  // namespace extensions
