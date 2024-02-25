// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_

#include <optional>
#include <string>

#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"

namespace enterprise_connectors {

// Definition of the SignalsDecorator common to all platforms.
class CommonSignalsDecorator : public SignalsDecorator {
 public:
  CommonSignalsDecorator();
  ~CommonSignalsDecorator() override;

  // SignalsDecorator:
  void Decorate(base::Value::Dict& signals,
                base::OnceClosure done_closure) override;

 private:
  void OnHardwareInfoRetrieved(base::Value::Dict& signals,
                               base::TimeTicks start_time,
                               base::OnceClosure done_closure,
                               base::SysInfo::HardwareInfo hardware_info);

  void UpdateFromCache(base::Value::Dict& signals);

  // These two signals are fetched asynchronously and their collection can
  // involve expensive operations such as reading from disk. Since these signals
  // are not expected to change throughout the browser's lifetime, they will be
  // cached in this decorator.
  std::optional<std::string> cached_device_model_;
  std::optional<std::string> cached_device_manufacturer_;

  base::WeakPtrFactory<CommonSignalsDecorator> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_
