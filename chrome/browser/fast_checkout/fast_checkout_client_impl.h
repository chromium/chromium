// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_

#include "chrome/browser/fast_checkout/fast_checkout_client.h"
#include "components/autofill_assistant/browser/public/headless_script_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

class FastCheckoutExternalActionDelegate;

// TODO(crbug.com/1338528): Add unit tests.
class FastCheckoutClientImpl
    : public content::WebContentsUserData<FastCheckoutClientImpl>,
      public FastCheckoutClient {
 public:
  ~FastCheckoutClientImpl() override;

  FastCheckoutClientImpl(const FastCheckoutClientImpl&) = delete;
  FastCheckoutClientImpl& operator=(const FastCheckoutClientImpl&) = delete;

  // FastCheckoutClient:
  bool Start(const GURL& url) override;
  void Stop() override;
  bool IsRunning() const override;

 protected:
  explicit FastCheckoutClientImpl(content::WebContents* web_contents);

  // Creates the external action deglegate and script controller.
  virtual std::unique_ptr<autofill_assistant::HeadlessScriptController>
  CreateHeadlessScriptController();

 private:
  friend class content::WebContentsUserData<FastCheckoutClientImpl>;

  // Registers when a run is complete. Used in callbacks.
  void OnRunComplete(
      autofill_assistant::HeadlessScriptController::ScriptResult result);

  // The delegate is responsible for handling protos received from backend DSL
  // actions.
  std::unique_ptr<FastCheckoutExternalActionDelegate>
      fast_checkout_external_action_delegate_;

  // Controls a script run triggered by the headless API. This class is
  // responsible for handling the forwarding of actions to
  // `apc_external_action_delegate_` and managing the run lifetime.
  std::unique_ptr<autofill_assistant::HeadlessScriptController>
      external_script_controller_;

  // True if a run is ongoing; used to avoid multiple runs in parallel.
  bool is_running_ = false;

  // content::WebContentsUserData:
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_
