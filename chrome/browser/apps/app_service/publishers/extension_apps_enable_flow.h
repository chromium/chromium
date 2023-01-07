// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_ENABLE_FLOW_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_ENABLE_FLOW_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"

class ExtensionEnableFlow;
class Profile;

namespace apps {

// A wrapper around ExtensionEnableFlow that attempts to enable an extension.
// The main utility this class provides is that the delegate overrides are
// wrapped in a callback, which allows the consumer to add additional state --
// e.g. adding state to track *which* extension has been enabled.
class ExtensionAppsEnableFlow : public ExtensionEnableFlowDelegate {
 public:
  ExtensionAppsEnableFlow(Profile* profile, const std::string& app_id);
  ~ExtensionAppsEnableFlow() override;

  ExtensionAppsEnableFlow(const ExtensionAppsEnableFlow&) = delete;
  ExtensionAppsEnableFlow& operator=(const ExtensionAppsEnableFlow&) = delete;

  // The callback contains a single parameter which describes whether the
  // extension was successfully enabled.
  using FinishedCallback = base::OnceCallback<void(bool)>;

  // Starts the process of enabling the extension. This can synchronously invoke
  // the callback if the extension is already enabled. Calling this method
  // multiple times before the previous callbacks have been invoked will
  // override the previous callbacks. Only the latest callback is stored.
  void Run(FinishedCallback callback);

 private:
  // ExtensionEnableFlowDelegate overrides.
  void ExtensionEnableFlowFinished() override;
  void ExtensionEnableFlowAborted(bool user_initiated) override;

  const raw_ptr<Profile> profile_;
  const std::string app_id_;
  FinishedCallback callback_;
  std::unique_ptr<ExtensionEnableFlow> flow_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_ENABLE_FLOW_H_
