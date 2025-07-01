// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/mojom/context_type.mojom.h"

namespace extensions {
namespace {

class ManagementUninstallFunctionUninstallDialogDelegate
    : public ExtensionUninstallDialog::Delegate,
      public UninstallDialogDelegate {
 public:
  ManagementUninstallFunctionUninstallDialogDelegate(
      ManagementUninstallFunctionBase* function,
      const Extension* target_extension,
      bool show_programmatic_uninstall_ui)
      : function_(function) {
    ChromeExtensionFunctionDetails details(function);
    extension_uninstall_dialog_ = ExtensionUninstallDialog::Create(
        Profile::FromBrowserContext(function->browser_context()),
        details.GetNativeWindowForUI(), this);
    bool uninstall_from_webstore =
        (function->extension() &&
         function->extension()->id() == kWebStoreAppId) ||
        function->source_url().DomainIs(
            extension_urls::GetNewWebstoreLaunchURL().host());
    UninstallSource source;
    UninstallReason reason;
    if (uninstall_from_webstore) {
      source = UNINSTALL_SOURCE_CHROME_WEBSTORE;
      reason = UNINSTALL_REASON_CHROME_WEBSTORE;
    } else if (function->source_context_type() == mojom::ContextType::kWebUi) {
      source = UNINSTALL_SOURCE_CHROME_EXTENSIONS_PAGE;
      // TODO: Update this to a new reason; it shouldn't be lumped in with
      // other uninstalls if it's from the chrome://extensions page.
      reason = UNINSTALL_REASON_MANAGEMENT_API;
    } else {
      source = UNINSTALL_SOURCE_EXTENSION;
      reason = UNINSTALL_REASON_MANAGEMENT_API;
    }
    if (show_programmatic_uninstall_ui) {
      extension_uninstall_dialog_->ConfirmUninstallByExtension(
          target_extension, function->extension(), reason, source);
    } else {
      extension_uninstall_dialog_->ConfirmUninstall(target_extension, reason,
                                                    source);
    }
  }

  ManagementUninstallFunctionUninstallDialogDelegate(
      const ManagementUninstallFunctionUninstallDialogDelegate&) = delete;
  ManagementUninstallFunctionUninstallDialogDelegate& operator=(
      const ManagementUninstallFunctionUninstallDialogDelegate&) = delete;

  ~ManagementUninstallFunctionUninstallDialogDelegate() override = default;

  // ExtensionUninstallDialog::Delegate implementation.
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const std::u16string& error) override {
    function_->OnExtensionUninstallDialogClosed(did_start_uninstall, error);
  }

 private:
  raw_ptr<ManagementUninstallFunctionBase> function_;
  std::unique_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;
};

}  // namespace

std::unique_ptr<UninstallDialogDelegate>
ChromeManagementAPIDelegate::UninstallFunctionDelegate(
    ManagementUninstallFunctionBase* function,
    const Extension* target_extension,
    bool show_programmatic_uninstall_ui) const {
  return std::make_unique<ManagementUninstallFunctionUninstallDialogDelegate>(
      function, target_extension, show_programmatic_uninstall_ui);
}

}  // namespace extensions
