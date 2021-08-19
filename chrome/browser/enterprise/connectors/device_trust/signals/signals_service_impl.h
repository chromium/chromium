// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service.h"

namespace enterprise_connectors {

class SignalsDecorator;

class SignalsServiceImpl : public SignalsService {
 public:
  explicit SignalsServiceImpl(
      std::vector<std::unique_ptr<SignalsDecorator>> signals_decorators);

  SignalsServiceImpl(const SignalsServiceImpl&) = delete;
  SignalsServiceImpl& operator=(const SignalsServiceImpl&) = delete;

  ~SignalsServiceImpl() override;

  // SignalsService:
  std::unique_ptr<DeviceTrustSignals> CollectSignals() override;

 private:
  std::vector<std::unique_ptr<SignalsDecorator>> signals_decorators_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_IMPL_H_
