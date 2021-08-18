// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_factory.h"

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/common_signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_impl.h"

namespace enterprise_connectors {

std::unique_ptr<SignalsService> CreateSignalsService() {
  std::vector<std::unique_ptr<SignalsDecorator>> decorators;

  decorators.push_back(std::make_unique<CommonSignalsDecorator>());

  return std::make_unique<SignalsServiceImpl>(std::move(decorators));
}

}  // namespace enterprise_connectors
