// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_disabled_ui.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/extension_install_error_menu_item_id_provider.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"

// ExtensionDisabledGlobalError -----------------------------------------------

namespace extensions {

class ExtensionDisabledGlobalError final
    : public GlobalErrorWithStandardBubble,
      public ExtensionUninstallDialog::Delegate,
      public ExtensionRegistryObserver {
 public:
  ExtensionDisabledGlobalError(ExtensionService* service,
                               const Extension* extension,
                               bool is_remote_install);

  ExtensionDisabledGlobalError(const ExtensionDisabledGlobalError&) = delete;
  ExtensionDisabledGlobalError& operator=(const ExtensionDisabledGlobalError&) =
      delete;

  ~ExtensionDisabledGlobalError() override;

  // GlobalError:
  Severity GetSeverity() override;
  bool HasMenuItem() override;
  int MenuItemCommandID() override;
  std::u16string MenuItemLabel() override;
  void ExecuteMenuItem(Browser* browser) override;
  std::u16string GetBubbleViewTitle() override;
  std::vector<std::u16string> GetBubbleViewMessages() override;
  std::u16string GetBubbleViewAcceptButtonLabel() override;
  std::u16string GetBubbleViewCancelButtonLabel() override;
  void OnBubbleViewDidClose(Browser* browser) override {}
  void BubbleViewAcceptButtonPressed(Browser* browser) override;
  void BubbleViewCancelButtonPressed(Browser* browser) override;
  base::WeakPtr<GlobalErrorWithStandardBubble> AsWeakPtr() override;
  bool ShouldCloseOnDeactivate() const override;
  bool ShouldShowCloseButton() const override;

  // ExtensionUninstallDialog::Delegate:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const std::u16string& error) override;

 private:
  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  void RemoveGlobalError();

  raw_ptr<ExtensionService, DanglingUntriaged> service_;
  scoped_refptr<const Extension> extension_;
  bool is_remote_install_;

  std::unique_ptr<ExtensionUninstallDialog> uninstall_dialog_;

  // Helper to get menu command ID assigned for this extension's error.
  ExtensionInstallErrorMenuItemIdProvider id_provider_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};

  base::WeakPtrFactory<ExtensionDisabledGlobalError> weak_ptr_factory_{this};
};

// TODO(yoz): create error at startup for disabled extensions.
ExtensionDisabledGlobalError::ExtensionDisabledGlobalError(
    ExtensionService* service,
    const Extension* extension,
    bool is_remote_install)
    : service_(service),
      extension_(extension),
      is_remote_install_(is_remote_install) {
  registry_observation_.Observe(ExtensionRegistry::Get(service->profile()));
}

ExtensionDisabledGlobalError::~ExtensionDisabledGlobalError() {}

GlobalError::Severity ExtensionDisabledGlobalError::GetSeverity() {
  return SEVERITY_LOW;
}

bool ExtensionDisabledGlobalError::HasMenuItem() {
  return true;
}

int ExtensionDisabledGlobalError::MenuItemCommandID() {
  return id_provider_.menu_command_id();
}

std::u16string ExtensionDisabledGlobalError::MenuItemLabel() {
  std::string extension_name = extension_->name();
  // Ampersands need to be escaped to avoid being treated like
  // mnemonics in the menu.
  base::ReplaceChars(extension_name, "&", "&&", &extension_name);

  if (is_remote_install_) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_DISABLED_REMOTE_INSTALL_ERROR_TITLE,
        base::UTF8ToUTF16(extension_name));
  } else {
    return l10n_util::GetStringFUTF16(IDS_EXTENSION_DISABLED_ERROR_TITLE,
                                      base::UTF8ToUTF16(extension_name));
  }
}

void ExtensionDisabledGlobalError::ExecuteMenuItem(Browser* browser) {
  ShowBubbleView(browser);
}

std::u16string ExtensionDisabledGlobalError::GetBubbleViewTitle() {
  if (is_remote_install_) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_DISABLED_REMOTE_INSTALL_ERROR_TITLE,
        base::UTF8ToUTF16(extension_->name()));
  } else {
    return l10n_util::GetStringFUTF16(IDS_EXTENSION_DISABLED_ERROR_TITLE,
                                      base::UTF8ToUTF16(extension_->name()));
  }
}

std::vector<std::u16string>
ExtensionDisabledGlobalError::GetBubbleViewMessages() {
  std::vector<std::u16string> messages;

  std::unique_ptr<const PermissionSet> granted_permissions =
      ExtensionPrefs::Get(service_->GetBrowserContext())
          ->GetGrantedPermissions(extension_->id());

  PermissionMessages permission_warnings =
      extension_->permissions_data()->GetNewPermissionMessages(
          *granted_permissions);

  if (is_remote_install_) {
    if (!permission_warnings.empty())
      messages.push_back(
          l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO));
  } else {
    // TODO(crbug.com/40406971): If NeedCustodianApprovalForPermissionIncrease,
    // add an extra message for supervised users.
    messages.push_back(
        l10n_util::GetStringUTF16(IDS_EXTENSION_DISABLED_ERROR_LABEL));
  }
  for (const PermissionMessage& msg : permission_warnings) {
    messages.push_back(l10n_util::GetStringFUTF16(IDS_EXTENSION_PERMISSION_LINE,
                                                  msg.message()));
  }
  return messages;
}

std::u16string ExtensionDisabledGlobalError::GetBubbleViewAcceptButtonLabel() {
  if (is_remote_install_) {
    return l10n_util::GetStringUTF16(
        extension_->is_app()
            ? IDS_EXTENSION_PROMPT_REMOTE_INSTALL_BUTTON_APP
            : IDS_EXTENSION_PROMPT_REMOTE_INSTALL_BUTTON_EXTENSION);
  }
  return l10n_util::GetStringUTF16(
      IDS_EXTENSION_PROMPT_PERMISSIONS_ACCEPT_BUTTON);
}

std::u16string ExtensionDisabledGlobalError::GetBubbleViewCancelButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON);
}

void ExtensionDisabledGlobalError::BubbleViewAcceptButtonPressed(
    Browser* browser) {
  // Delay extension reenabling so this bubble closes properly.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExtensionService::GrantPermissionsAndEnableExtension,
                     service_->AsExtensionServiceWeakPtr(),
                     base::RetainedRef(extension_)));
}

void ExtensionDisabledGlobalError::BubbleViewCancelButtonPressed(
    Browser* browser) {
  uninstall_dialog_ = ExtensionUninstallDialog::Create(
      service_->profile(), browser->window()->GetNativeWindow(), this);
  // Delay showing the uninstall dialog, so that this function returns
  // immediately, to close the bubble properly. See crbug.com/121544.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ExtensionUninstallDialog::ConfirmUninstall,
                                uninstall_dialog_->AsWeakPtr(),
                                base::RetainedRef(extension_),
                                UNINSTALL_REASON_EXTENSION_DISABLED,
                                UNINSTALL_SOURCE_PERMISSIONS_INCREASE));
}

base::WeakPtr<GlobalErrorWithStandardBubble>
ExtensionDisabledGlobalError::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool ExtensionDisabledGlobalError::ShouldCloseOnDeactivate() const {
  // Since this indicates that an extension was disabled, we should definitely
  // have the user acknowledge it, rather than having the bubble disappear when
  // a new window pops up.
  return false;
}

bool ExtensionDisabledGlobalError::ShouldShowCloseButton() const {
  // As we don't close the bubble on deactivation (see ShouldCloseOnDeactivate),
  // we add a close button so the user doesn't *need* to act right away.
  // If the bubble is closed, the error remains in the wrench menu and the user
  // can address it later.
  return true;
}

void ExtensionDisabledGlobalError::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const std::u16string& error) {
  // No need to do anything.
}

void ExtensionDisabledGlobalError::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension != extension_)
    return;
  RemoveGlobalError();
}

void ExtensionDisabledGlobalError::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  if (extension != extension_)
    return;
  RemoveGlobalError();
}

void ExtensionDisabledGlobalError::OnShutdown(ExtensionRegistry* registry) {
  DCHECK_EQ(ExtensionRegistry::Get(service_->profile()), registry);
  registry_observation_.Reset();
}

void ExtensionDisabledGlobalError::RemoveGlobalError() {
  std::unique_ptr<GlobalError> ptr =
      GlobalErrorServiceFactory::GetForProfile(service_->profile())
          ->RemoveGlobalError(this);
  registry_observation_.Reset();
  // Delete this object after any running tasks, so that the extension dialog
  // still has it as a delegate to finish the current tasks.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                ptr.release());
}

// Globals --------------------------------------------------------------------

void AddExtensionDisabledError(ExtensionService* service,
                               const Extension* extension,
                               bool is_remote_install) {
  if (extension) {
    GlobalErrorServiceFactory::GetForProfile(service->profile())
        ->AddGlobalError(std::make_unique<ExtensionDisabledGlobalError>(
            service, extension, is_remote_install));
  }
}

}  // namespace extensions
