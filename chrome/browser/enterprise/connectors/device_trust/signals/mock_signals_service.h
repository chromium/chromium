// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_MOCK_SIGNALS_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_MOCK_SIGNALS_SERVICE_H_

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {
namespace test {

class MockSignalsService : public SignalsService {
 public:
  MockSignalsService();
  ~MockSignalsService() override;

  MOCK_METHOD(void, CollectSignals, (CollectSignalsCallback), (override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_MOCK_SIGNALS_SERVICE_H_
