// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/mock_signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

using test::MockSignalsDecorator;
using ::testing::_;

TEST(SignalsServiceImplTest, CollectSignals_CallsAllDecorators) {
  std::string fake_obfuscated_customer_id = "fake_obfuscated_customer_id";
  std::unique_ptr<MockSignalsDecorator> first_decorator =
      std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*first_decorator.get(), Decorate(_))
      .WillOnce([&fake_obfuscated_customer_id](DeviceTrustSignals& signals) {
        signals.set_obfuscated_customer_id(fake_obfuscated_customer_id);
      });

  std::string fake_device_id = "fake_device_id";
  std::unique_ptr<MockSignalsDecorator> second_decorator =
      std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*second_decorator.get(), Decorate(_))
      .WillOnce([&fake_device_id](DeviceTrustSignals& signals) {
        signals.set_device_id(fake_device_id);
      });

  std::vector<std::unique_ptr<SignalsDecorator>> decorators;
  decorators.push_back(std::move(first_decorator));
  decorators.push_back(std::move(second_decorator));

  SignalsServiceImpl service(std::move(decorators));

  std::unique_ptr<DeviceTrustSignals> signals = service.CollectSignals();

  EXPECT_EQ(signals->obfuscated_customer_id(), fake_obfuscated_customer_id);
  EXPECT_EQ(signals->device_id(), fake_device_id);
}

}  // namespace enterprise_connectors
