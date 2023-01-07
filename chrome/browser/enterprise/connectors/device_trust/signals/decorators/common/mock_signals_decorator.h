// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_MOCK_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_MOCK_SIGNALS_DECORATOR_H_

#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {
namespace test {

class MockSignalsDecorator : public SignalsDecorator {
 public:
  MockSignalsDecorator();
  ~MockSignalsDecorator() override;

  MOCK_METHOD(void,
              Decorate,
              (base::Value::Dict&, base::OnceClosure),
              (override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_MOCK_SIGNALS_DECORATOR_H_
