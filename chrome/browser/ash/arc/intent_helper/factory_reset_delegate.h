// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INTENT_HELPER_FACTORY_RESET_DELEGATE_H_
#define CHROME_BROWSER_ASH_ARC_INTENT_HELPER_FACTORY_RESET_DELEGATE_H_

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

namespace arc {

// Implementation of ArcIntentHelperBridge::Delegate interface.
class FactoryResetDelegate : public ArcIntentHelperBridge::Delegate {
 public:
  FactoryResetDelegate();
  ~FactoryResetDelegate() override;

  // ArcIntentHelperBridge::Delegate:
  void ResetArc() override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INTENT_HELPER_FACTORY_RESET_DELEGATE_H_
