// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service.h"

namespace enterprise_connectors {

class SignalsDecorator;
class SignalsFilterer;

class SignalsServiceImpl : public SignalsService {
 public:
  SignalsServiceImpl(
      std::vector<std::unique_ptr<SignalsDecorator>> signals_decorators,
      std::unique_ptr<SignalsFilterer> signals_filterer);

  SignalsServiceImpl(const SignalsServiceImpl&) = delete;
  SignalsServiceImpl& operator=(const SignalsServiceImpl&) = delete;

  ~SignalsServiceImpl() override;

  // SignalsService:
  void CollectSignals(CollectSignalsCallback callback) override;

 private:
  void OnSignalsDecorated(CollectSignalsCallback callback,
                          base::TimeTicks start_time,
                          std::unique_ptr<base::Value::Dict> signals);

  std::vector<std::unique_ptr<SignalsDecorator>> signals_decorators_;
  std::unique_ptr<SignalsFilterer> signals_filterer_;

  base::WeakPtrFactory<SignalsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_IMPL_H_
