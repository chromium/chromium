// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_CONTEXT_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_CONTEXT_SIGNALS_DECORATOR_H_

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"

namespace enterprise_signals {
class ContextInfoFetcher;
struct ContextInfo;
}  // namespace enterprise_signals

namespace enterprise_connectors {

// Definition of the SignalsDecorator that uses the ContextInfoFetcher to get
// device signals. The signal collected here are supported on all platforms and
// are equivalent to the signals exposed through the
// chrome.enterprise.reportingPrivate.getContextInfo private extension API.
class ContextSignalsDecorator : public SignalsDecorator {
 public:
  explicit ContextSignalsDecorator(
      std::unique_ptr<enterprise_signals::ContextInfoFetcher>
          context_info_fetcher);
  ~ContextSignalsDecorator() override;

  // SignalsDecorator:
  void Decorate(base::Value::Dict& signals,
                base::OnceClosure done_closure) override;

 private:
  void OnSignalsFetched(base::Value::Dict& signals,
                        base::TimeTicks start_time,
                        base::OnceClosure done_closure,
                        enterprise_signals::ContextInfo context_info);

  std::unique_ptr<enterprise_signals::ContextInfoFetcher> context_info_fetcher_;

  base::WeakPtrFactory<ContextSignalsDecorator> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_CONTEXT_SIGNALS_DECORATOR_H_
