// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "android_webview/browser/aw_field_trial_creator.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/browser_main_parts.h"

namespace base {
class MessageLoop;
}

namespace policy {
class BrowserPolicyConnectorBase;
}

namespace android_webview {

class AwContentBrowserClient;

class AwBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit AwBrowserMainParts(AwContentBrowserClient* browser_client);
  ~AwBrowserMainParts() override;

  // Overriding methods from content::BrowserMainParts.
  int PreEarlyInitialization() override;
  int PreCreateThreads() override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void ServiceManagerConnectionStarted(
      content::ServiceManagerConnection* connection) override;

 private:
  // Android specific UI MessageLoop.
  std::unique_ptr<base::MessageLoop> main_message_loop_;

  // Created and temporarily owned by AwBrowserMainParts
  // until ownership is transferred to AwBrowserContext.
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<policy::BrowserPolicyConnectorBase> browser_policy_connector_;

  AwContentBrowserClient* browser_client_;

  // Responsible for creating a feature list from the seed. This object must
  // exist for the lifetime of the process as it contains the FieldTrialList
  // that can be queried for the state of experiments.
  AwFieldTrialCreator aw_field_trial_creator_;

  DISALLOW_COPY_AND_ASSIGN(AwBrowserMainParts);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_
