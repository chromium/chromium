// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/common_signals_decorator.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/system/fake_statistics_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors {

TEST(CommonSignalsDecoratorTest, Decorate) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  CommonSignalsDecorator decorator;

  DeviceTrustSignals signals;
  decorator.Decorate(signals);

  EXPECT_TRUE(signals.has_os());
  EXPECT_TRUE(signals.has_os_version());
  EXPECT_TRUE(signals.has_device_model());
  EXPECT_TRUE(signals.has_device_manufacturer());
  EXPECT_TRUE(signals.has_display_name());
}

}  // namespace enterprise_connectors
