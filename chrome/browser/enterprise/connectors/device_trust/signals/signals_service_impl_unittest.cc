// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/mock_signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

using test::MockSignalsDecorator;
using ::testing::_;

namespace {

constexpr char kLatencyHistogram[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.Full";

}  // namespace

TEST(SignalsServiceImplTest, CollectSignals_CallsAllDecorators) {
  base::HistogramTester histogram_tester;
  std::string fake_obfuscated_customer_id = "fake_obfuscated_customer_id";
  std::unique_ptr<MockSignalsDecorator> first_decorator =
      std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*first_decorator.get(), Decorate(_, _))
      .WillOnce([&fake_obfuscated_customer_id](SignalsType& signals,
                                               base::OnceClosure done_closure) {
        signals.set_obfuscated_customer_id(fake_obfuscated_customer_id);
        std::move(done_closure).Run();
      });

  std::string fake_device_id = "fake_device_id";
  std::unique_ptr<MockSignalsDecorator> second_decorator =
      std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*second_decorator.get(), Decorate(_, _))
      .WillOnce([&fake_device_id](SignalsType& signals,
                                  base::OnceClosure done_closure) {
        signals.set_device_id(fake_device_id);
        std::move(done_closure).Run();
      });

  std::vector<std::unique_ptr<SignalsDecorator>> decorators;
  decorators.push_back(std::move(first_decorator));
  decorators.push_back(std::move(second_decorator));

  SignalsServiceImpl service(std::move(decorators));

  bool callback_called = false;
  auto callback =
      base::BindLambdaForTesting([&](std::unique_ptr<SignalsType> signals) {
        EXPECT_EQ(signals->obfuscated_customer_id(),
                  fake_obfuscated_customer_id);
        EXPECT_EQ(signals->device_id(), fake_device_id);
        callback_called = true;
      });

  service.CollectSignals(std::move(callback));

  EXPECT_TRUE(callback_called);
  histogram_tester.ExpectTotalCount(kLatencyHistogram, 1);
}

}  // namespace enterprise_connectors
