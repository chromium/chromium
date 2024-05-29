// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_manager.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/supervised_user/chromeos/parent_access_extension_approvals_manager.h"
#endif

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

class ParentPermissionDialog;

namespace extensions {

class ExtensionIconLoader;
enum class ExtensionInstalledBlockedByParentDialogAction;

// Handles extensions approvals for supervised users.
// Decides which version of the flow should be used and dispatches the calls
// accordingly. Currently we support two flow versions.
// - A browser dialog, which is shown for non-ChromeOS desktop platforms.
// - A system component dialog implemented for ChromeOS.
class SupervisedUserExtensionsDelegateImpl
    : public SupervisedUserExtensionsDelegate {
 public:
  explicit SupervisedUserExtensionsDelegateImpl(
      content::BrowserContext* context);
  ~SupervisedUserExtensionsDelegateImpl() override;

  // SupervisedUserExtensionsDelegate overrides
  bool IsChild() const override;
  bool IsExtensionAllowedByParent(const Extension& extension) const override;
  void RequestToAddExtensionOrShowError(
      const Extension& extension,
      content::WebContents* web_contents,
      const gfx::ImageSkia& icon,
      SupervisedUserExtensionParentApprovalEntryPoint
          extension_approval_entry_point,
      ExtensionApprovalDoneCallback extension_approval_callback) override;
  void RequestToEnableExtensionOrShowError(
      const Extension& extension,
      content::WebContents* web_contents,
      SupervisedUserExtensionParentApprovalEntryPoint
          extension_approval_entry_point,
      ExtensionApprovalDoneCallback extension_approval_callback) override;
  void UpdateManagementPolicyRegistration() override;
  bool CanInstallExtensions() const override;
  void AddExtensionApproval(const extensions::Extension& extension) override;
  void MaybeRecordPermissionsIncreaseMetrics(
      const extensions::Extension& extension) override;
  void RemoveExtensionApproval(const extensions::Extension& extension) override;
  void RecordExtensionEnablementUmaMetrics(bool enabled) const override;

 private:
  // Shows a ParentPermissionDialog for |extension| and calls
  // |done_callback| when it completes. Called for non-ChromeOS desktop
  // platforms.
  void ShowParentPermissionDialogForExtension(
      const Extension& extension,
      content::WebContents* contents,
      const gfx::ImageSkia& icon,
      SupervisedUserExtensionParentApprovalEntryPoint
          extension_approval_entry_point);

  // Shows ParentPermissionDialog indicating that |extension| has been blocked
  // and call |done_callback| when it completes. Depending on the blocked_action
  // type, the UI of the dialog may differ. Called for desktop non-ChromeOS
  // platforms.
  void ShowInstallBlockedByParentDialogForExtension(
      const Extension& extension,
      content::WebContents* contents,
      ExtensionInstalledBlockedByParentDialogAction blocked_action);

  // This method is called after all async data are fetched.
  // Since `WebContents` that initiated the request could be destroyed during
  // the async fetch the method cancels the request if `contents` was specified,
  // but is not valid anymore. `contents` is an optional argument because the
  // request can be made from an entry point not associate with `WebContents`.
  // This method decides which version of the flow to start.
  // On non-ChromeOS desktop platforms, a browser permission dialog or
  // browser blocked dialog is shown. On ChromeOS, ParentAccessDialog is
  // shown. The widget handles blocked state internally.
  void RequestExtensionApproval(
      const Extension& extension,
      std::optional<base::WeakPtr<content::WebContents>> contents,
      SupervisedUserExtensionParentApprovalEntryPoint
          extension_approval_entry_point,
      const gfx::ImageSkia& icon);

  // The ParentPermissionDialog pointer is only destroyed when a new dialog is
  // created or the SupervisedUserExtensionsDelegate is destroyed. Therefore
  // there can only be one dialog opened at a time and the last dialog object
  // can have a pretty long lifetime.
  // TODO(b/278874130): Move non ChromeOS platform-specific code to its own
  // class for clearer distinction.
  std::unique_ptr<ParentPermissionDialog> parent_permission_dialog_;

  SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback
      done_callback_;

  std::unique_ptr<ExtensionIconLoader> icon_loader_;

  const raw_ptr<content::BrowserContext> context_;

#if BUILDFLAG(IS_CHROMEOS)
  // Manages the ChromeOS-specific approval flow.
  // The extension approvals manager is destroyed when a new ParentAccessDialog
  // is created or this delegate is destroyed.
  std::unique_ptr<ParentAccessExtensionApprovalsManager>
      extension_approvals_manager_;
#endif
  SupervisedUserExtensionsManager extensions_manager_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
