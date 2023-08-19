// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_filterer.h"

#include "components/device_signals/core/common/signals_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {
constexpr char kFakeSerialNumber[] = "fake_serial_number";
}  // namespace

// Test that nothing is filtered
TEST(SignalsFiltererTest, Filter_DoNothing) {
  base::Value::Dict dict;
  dict.Set(device_signals::names::kSerialNumber, kFakeSerialNumber);

  SignalsFilterer filterer;
  filterer.Filter(dict);

  EXPECT_EQ(*dict.FindString(device_signals::names::kSerialNumber),
            kFakeSerialNumber);
}

}  // namespace enterprise_connectors
