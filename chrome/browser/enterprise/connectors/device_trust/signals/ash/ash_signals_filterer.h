// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_ASH_ASH_SIGNALS_FILTERER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_ASH_ASH_SIGNALS_FILTERER_H_

#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_filterer.h"

namespace enterprise_connectors {

// This class is in charge of removing or modifyinh signals from the signal
// payload on ChromeOS Ash.
class AshSignalsFilterer : public SignalsFilterer {
 public:
  ~AshSignalsFilterer() override;

  // Removes or modifies a subset of `signals` depending on the current device
  // context.
  void Filter(base::Value::Dict& signals) override;

 private:
  // Removes all signals which contain stable device identifiers from `signals`.
  void RemoveStableDeviceIdentifiers(base::Value::Dict& signals);

  bool ShouldRemoveStableDeviceIdentifiers();
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_ASH_ASH_SIGNALS_FILTERER_H_
