// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_EXTERNAL_ACTION_DELEGATE_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_EXTERNAL_ACTION_DELEGATE_H_

#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "content/public/browser/web_contents.h"

// TODO(crbug.com/1338529): Add unit tests.

// Handles external actions defined for fast checkout.
class FastCheckoutExternalActionDelegate
    : public autofill_assistant::ExternalActionDelegate {
 public:
  FastCheckoutExternalActionDelegate() = default;
  ~FastCheckoutExternalActionDelegate() override;

  FastCheckoutExternalActionDelegate(
      const FastCheckoutExternalActionDelegate&) = delete;
  FastCheckoutExternalActionDelegate& operator=(
      const FastCheckoutExternalActionDelegate&) = delete;

  // ExternalActionDelegate:
  void OnActionRequested(
      const autofill_assistant::external::Action& action_info,
      base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
      base::OnceCallback<void(const autofill_assistant::external::Result&)>
          end_action_callback) override;

  void OnInterruptStarted() override;
  void OnInterruptFinished() override;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_EXTERNAL_ACTION_DELEGATE_H_
