// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_WAIT_FOR_NETWORK_CALLBACK_HELPER_ASH_H_
#define CHROME_BROWSER_SIGNIN_WAIT_FOR_NETWORK_CALLBACK_HELPER_ASH_H_

#include "base/functional/callback_forward.h"
#include "components/signin/public/base/wait_for_network_callback_helper.h"

class WaitForNetworkCallbackHelperAsh : public WaitForNetworkCallbackHelper {
 public:
  // WaitForNetworkCallbackHelper:
  bool AreNetworkCallsDelayed() override;
  void DelayNetworkCall(base::OnceClosure callback) override;
  void DisableNetworkCallsDelayedForTesting(bool disable) override;

 private:
  bool delaying_network_calls_disabled_for_testing_ = false;
};

#endif  // CHROME_BROWSER_SIGNIN_WAIT_FOR_NETWORK_CALLBACK_HELPER_ASH_H_
