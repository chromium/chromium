// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

class SupervisedUserExtensionsDelegateImpl
    : public SupervisedUserExtensionsDelegate {
 public:
  explicit SupervisedUserExtensionsDelegateImpl(
      content::BrowserContext* browser_context);
  ~SupervisedUserExtensionsDelegateImpl() override;

  // SupervisedUserExtensionsDelegate overrides
  bool IsChild() const override;
  bool IsExtensionAllowedByParent(const Extension& extension) const override;
  void RequestToAddExtensionOrShowError(
      const Extension& extension,
      content::WebContents* web_contents,
      const gfx::ImageSkia& icon,
      ExtensionApprovalDoneCallback extension_approval_callback) override;
  void RequestToEnableExtensionOrShowError(
      const Extension& extension,
      content::WebContents* web_contents,
      ExtensionApprovalDoneCallback extension_approval_callback) override;

 private:
  // Returns true if |context_| represents a supervised child account who may
  // install extensions with parent permission.
  bool CanInstallExtensions() const;

  // Shows a parent permission dialog for |extension| and call |done_callback|
  // when it completes.
  void ShowParentPermissionDialogForExtension(const Extension& extension,
                                              content::WebContents* contents,
                                              const gfx::ImageSkia& icon);

  // Shows a dialog indicating that |extension| has been blocked and call
  // |done_callback| when it completes. Depending on the blocked_action type,
  // the UI of the dialog may differ.
  void ShowInstallBlockedByParentDialogForExtension(
      const Extension& extension,
      content::WebContents* contents,
      ExtensionInstalledBlockedByParentDialogAction blocked_action);

  // Shows the ParentAccessDialog if V2 flag is enabled. If V2 is not enabled,
  // the ParentPermissionDialog is shown if extension installation is not
  // blocked. If permission is blocked, the block dialog is shown.
  void RequestExtensionApproval(const Extension& extension,
                                content::WebContents* contents,
                                const gfx::ImageSkia& icon);

  // The dialog pointer is only destroyed when a new dialog is created or the
  // SupervisedUserExtensionsDelegate is destroyed. Therefore there can only be
  // one dialog opened at a time and the last dialog object can have a pretty
  // long lifetime.
  std::unique_ptr<ParentPermissionDialog> parent_permission_dialog_;

  SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback
      done_callback_;

  std::unique_ptr<ExtensionIconLoader> icon_loader_;
  const raw_ptr<content::BrowserContext> context_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The extension approvals manage is destroyed when a new ParentAccessDialog
  // is created or this delegate is destroyed.
  std::unique_ptr<ParentAccessExtensionApprovalsManager>
      extension_approvals_manager_;
#endif
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
