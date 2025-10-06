// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/launch_util.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/mojom/context_type.mojom.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

class ManagementSetEnabledFunctionInstallPromptDelegate
    : public InstallPromptDelegate {
 public:
  ManagementSetEnabledFunctionInstallPromptDelegate(
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const Extension* extension,
      base::OnceCallback<void(bool)> callback)
      : install_prompt_(new ExtensionInstallPrompt(web_contents)),
        callback_(std::move(callback)) {
    ExtensionInstallPrompt::PromptType type =
        ExtensionInstallPrompt::GetReEnablePromptTypeForExtension(
            browser_context, extension);
    install_prompt_->ShowDialog(
        base::BindOnce(&ManagementSetEnabledFunctionInstallPromptDelegate::
                           OnInstallPromptDone,
                       weak_factory_.GetWeakPtr()),
        extension, nullptr,
        std::make_unique<ExtensionInstallPrompt::Prompt>(type),
        ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  }

  ManagementSetEnabledFunctionInstallPromptDelegate(
      const ManagementSetEnabledFunctionInstallPromptDelegate&) = delete;
  ManagementSetEnabledFunctionInstallPromptDelegate& operator=(
      const ManagementSetEnabledFunctionInstallPromptDelegate&) = delete;

  ~ManagementSetEnabledFunctionInstallPromptDelegate() override = default;

 private:
  void OnInstallPromptDone(
      ExtensionInstallPrompt::DoneCallbackPayload payload) {
    // This dialog doesn't support the "withhold permissions" checkbox.
    DCHECK_NE(
        payload.result,
        ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS);
    std::move(callback_).Run(payload.result ==
                             ExtensionInstallPrompt::Result::ACCEPTED);
  }

  // Used for prompting to re-enable items with permissions escalation updates.
  std::unique_ptr<ExtensionInstallPrompt> install_prompt_;

  base::OnceCallback<void(bool)> callback_;

  base::WeakPtrFactory<ManagementSetEnabledFunctionInstallPromptDelegate>
      weak_factory_{this};
};

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
            extension_urls::GetNewWebstoreLaunchURL().GetHost());
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

SupervisedUserExtensionsDelegate*
GetSupervisedUserExtensionsDelegateFromContext(
    content::BrowserContext* context) {
  SupervisedUserExtensionsDelegate* supervised_user_extensions_delegate =
      ManagementAPI::GetFactoryInstance()
          ->Get(context)
          ->GetSupervisedUserExtensionsDelegate();
  CHECK(supervised_user_extensions_delegate);
  return supervised_user_extensions_delegate;
}

}  // namespace

ChromeManagementAPIDelegate::ChromeManagementAPIDelegate() = default;

ChromeManagementAPIDelegate::~ChromeManagementAPIDelegate() = default;

GURL ChromeManagementAPIDelegate::GetFullLaunchURL(
    const Extension* extension) const {
  return AppLaunchInfo::GetFullLaunchURL(extension);
}

LaunchType ChromeManagementAPIDelegate::GetLaunchType(
    const ExtensionPrefs* prefs,
    const Extension* extension) const {
  return ::extensions::GetLaunchType(prefs, extension);
}

std::unique_ptr<InstallPromptDelegate>
ChromeManagementAPIDelegate::SetEnabledFunctionDelegate(
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const Extension* extension,
    base::OnceCallback<void(bool)> callback) const {
  return std::make_unique<ManagementSetEnabledFunctionInstallPromptDelegate>(
      web_contents, browser_context, extension, std::move(callback));
}

std::unique_ptr<UninstallDialogDelegate>
ChromeManagementAPIDelegate::UninstallFunctionDelegate(
    ManagementUninstallFunctionBase* function,
    const Extension* target_extension,
    bool show_programmatic_uninstall_ui) const {
  return std::make_unique<ManagementUninstallFunctionUninstallDialogDelegate>(
      function, target_extension, show_programmatic_uninstall_ui);
}

void ChromeManagementAPIDelegate::EnableExtension(
    content::BrowserContext* context,
    const ExtensionId& extension_id) const {
  const Extension* extension =
      ExtensionRegistry::Get(context)->GetExtensionById(
          extension_id, ExtensionRegistry::EVERYTHING);
  // The extension must exist as this method is invoked on enabling an extension
  // from the extensions management page (see `ManagementSetEnabledFunction`).
  CHECK(extension);

  SupervisedUserExtensionsDelegate* extensions_delegate =
      GetSupervisedUserExtensionsDelegateFromContext(context);
  extensions_delegate->MaybeRecordPermissionsIncreaseMetrics(*extension);
  extensions_delegate->RecordExtensionEnablementUmaMetrics(/*enabled=*/true);

  // If the extension was disabled for a permissions increase, the Management
  // API will have displayed a re-enable prompt to the user, so we know it's
  // safe to grant permissions here.
  ExtensionRegistrar::Get(context)->GrantPermissionsAndEnableExtension(
      *extension);
}

void ChromeManagementAPIDelegate::DisableExtension(
    content::BrowserContext* context,
    const Extension* source_extension,
    const ExtensionId& extension_id,
    disable_reason::DisableReason disable_reason) const {
  SupervisedUserExtensionsDelegate* extensions_delegate =
      GetSupervisedUserExtensionsDelegateFromContext(context);
  extensions_delegate->RecordExtensionEnablementUmaMetrics(/*enabled=*/false);
  ExtensionRegistrar::Get(context)->DisableExtensionWithSource(
      source_extension, extension_id, disable_reason);
}

bool ChromeManagementAPIDelegate::UninstallExtension(
    content::BrowserContext* context,
    const ExtensionId& transient_extension_id,
    UninstallReason reason,
    std::u16string* error) const {
  return extensions::ExtensionRegistrar::Get(context)->UninstallExtension(
      transient_extension_id, reason, error);
}

void ChromeManagementAPIDelegate::SetLaunchType(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    LaunchType launch_type) const {
  ::extensions::SetLaunchType(context, extension_id, launch_type);
}

GURL ChromeManagementAPIDelegate::GetIconURL(const Extension* extension,
                                             int icon_size,
                                             ExtensionIconSet::Match match,
                                             bool grayscale) const {
  return ExtensionIconSource::GetIconURL(extension, icon_size, match,
                                         grayscale);
}

GURL ChromeManagementAPIDelegate::GetEffectiveUpdateURL(
    const Extension& extension,
    content::BrowserContext* context) const {
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(context);
  return extension_management->GetEffectiveUpdateURL(extension);
}

}  // namespace extensions
