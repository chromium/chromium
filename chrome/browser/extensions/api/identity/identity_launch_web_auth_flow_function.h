// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_LAUNCH_WEB_AUTH_FLOW_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_LAUNCH_WEB_AUTH_FLOW_FUNCTION_H_

#include <string>

#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class IdentityLaunchWebAuthFlowFunction : public ExtensionFunction,
                                          public WebAuthFlow::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.launchWebAuthFlow",
                             EXPERIMENTAL_IDENTITY_LAUNCHWEBAUTHFLOW)

  IdentityLaunchWebAuthFlowFunction();

  // Tests may override extension_id.
  void InitFinalRedirectURLPrefixForTest(const std::string& extension_id);

 private:
  ~IdentityLaunchWebAuthFlowFunction() override;
  ResponseAction Run() override;

  // WebAuthFlow::Delegate implementation.
  void OnAuthFlowFailure(WebAuthFlow::Failure failure) override;
  void OnAuthFlowURLChange(const GURL& redirect_url) override;
  void OnAuthFlowTitleChange(const std::string& title) override {}

  // Helper to initialize final URL prefix.
  void InitFinalRedirectURLPrefix(const std::string& extension_id);

  std::unique_ptr<WebAuthFlow> auth_flow_;
  GURL final_url_prefix_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_LAUNCH_WEB_AUTH_FLOW_FUNCTION_H_
