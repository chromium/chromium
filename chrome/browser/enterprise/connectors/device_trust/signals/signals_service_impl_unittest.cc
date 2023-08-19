// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/mock_signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/mock_signals_filterer.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_filterer.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

using test::MockSignalsDecorator;
using test::MockSignalsFilterer;
using ::testing::_;

namespace {

constexpr char kLatencyHistogram[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.Full";

}  // namespace

TEST(SignalsServiceImplTest, CollectSignals_CallsAllDecorators) {
  base::HistogramTester histogram_tester;
  std::string fake_display_name = "fake_display_name";
  std::unique_ptr<MockSignalsDecorator> first_decorator =
      std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*first_decorator.get(), Decorate(_, _))
      .WillOnce([&fake_display_name](base::Value::Dict& signals,
                                     base::OnceClosure done_closure) {
        signals.Set(device_signals::names::kDisplayName, fake_display_name);
        std::move(done_closure).Run();
      });

  std::string fake_allow_lock_screen = "false";
  std::unique_ptr<MockSignalsDecorator> second_decorator =
      std::make_unique<MockSignalsDecorator>();
  EXPECT_CALL(*second_decorator.get(), Decorate(_, _))
      .WillOnce([&fake_allow_lock_screen](base::Value::Dict& signals,
                                          base::OnceClosure done_closure) {
        signals.Set(device_signals::names::kAllowScreenLock,
                    fake_allow_lock_screen);
        std::move(done_closure).Run();
      });

  std::vector<std::unique_ptr<SignalsDecorator>> decorators;
  decorators.push_back(std::move(first_decorator));
  decorators.push_back(std::move(second_decorator));

  std::unique_ptr<MockSignalsFilterer> signals_filterer =
      std::make_unique<MockSignalsFilterer>();
  EXPECT_CALL(*signals_filterer.get(), Filter(_))
      .WillOnce([](base::Value::Dict& signals) { return; });

  SignalsServiceImpl service(std::move(decorators),
                             std::move(signals_filterer));

  bool callback_called = false;
  auto callback =
      base::BindLambdaForTesting([&](const base::Value::Dict signals) {
        EXPECT_EQ(
            signals.FindString(device_signals::names::kDisplayName)->c_str(),
            fake_display_name);
        EXPECT_EQ(signals.FindString(device_signals::names::kAllowScreenLock)
                      ->c_str(),
                  fake_allow_lock_screen);
        callback_called = true;
      });

  service.CollectSignals(std::move(callback));

  EXPECT_TRUE(callback_called);
  histogram_tester.ExpectTotalCount(kLatencyHistogram, 1);
}

}  // namespace enterprise_connectors
