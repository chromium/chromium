// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_MOCK_SIGNALS_FILTERER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_MOCK_SIGNALS_FILTERER_H_

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_filterer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

class MockSignalsFilterer : public SignalsFilterer {
 public:
  MockSignalsFilterer();
  ~MockSignalsFilterer() override;

  MOCK_METHOD(void, Filter, (base::Value::Dict & signals), (override));
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_MOCK_SIGNALS_FILTERER_H_
