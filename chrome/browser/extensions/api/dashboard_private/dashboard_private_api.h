// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DASHBOARD_PRIVATE_DASHBOARD_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DASHBOARD_PRIVATE_DASHBOARD_PRIVATE_API_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/common/extensions/api/dashboard_private.h"
#include "extensions/browser/extension_function.h"

class SkBitmap;

namespace extensions {

class Extension;

class DashboardPrivateShowPermissionPromptForDelegatedInstallFunction
    : public ExtensionFunction,
      public WebstoreInstallHelper::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "dashboardPrivate.showPermissionPromptForDelegatedInstall",
      DASHBOARDPRIVATE_SHOWPERMISSIONPROMPTFORDELEGATEDINSTALL)

  DashboardPrivateShowPermissionPromptForDelegatedInstallFunction();

 private:
  using Params =
     api::dashboard_private::ShowPermissionPromptForDelegatedInstall::Params;

  ~DashboardPrivateShowPermissionPromptForDelegatedInstallFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  // WebstoreInstallHelper::Delegate:
  void OnWebstoreParseSuccess(
      const std::string& id,
      const SkBitmap& icon,
      std::unique_ptr<base::DictionaryValue> parsed_manifest) override;
  void OnWebstoreParseFailure(const std::string& id,
                              InstallHelperResultCode result,
                              const std::string& error_message) override;

  void OnInstallPromptDone(ExtensionInstallPrompt::Result result);

  ExtensionFunction::ResponseValue BuildResponse(
      api::dashboard_private::Result result,
      const std::string& error);
  std::unique_ptr<base::ListValue> CreateResults(
      api::dashboard_private::Result result) const;

  const Params::Details& details() const { return params_->details; }

  std::unique_ptr<Params> params_;

  // A dummy Extension object we create for the purposes of using
  // ExtensionInstallPrompt to prompt for confirmation of the install.
  scoped_refptr<Extension> dummy_extension_;

  std::unique_ptr<ExtensionInstallPrompt> install_prompt_;

  DISALLOW_COPY_AND_ASSIGN(
      DashboardPrivateShowPermissionPromptForDelegatedInstallFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DASHBOARD_PRIVATE_DASHBOARD_PRIVATE_API_H_

