// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

// Handles extensions approvals for supervised users.
// Decides which version of the flow should be used and dispatches the calls
// accordingly. Currently we support two flow versions.
// V1 flow uses browser dialog to get approval.
// V2 flow is platform specific and uses system component to get approval. It is
// implemented on ChromeOS only.
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
  bool CanInstallExtensions() const override;
  void AddExtensionApproval(const extensions::Extension& extension) override;
  void RemoveExtensionApproval(const extensions::Extension& extension) override;
  void RecordExtensionEnablementUmaMetrics(bool enabled) const override;

 private:
  // Shows a parent permission V1 dialog for |extension| and call
  // |done_callback| when it completes.
  void ShowParentPermissionDialogForExtension(const Extension& extension,
                                              content::WebContents* contents,
                                              const gfx::ImageSkia& icon);

  // Shows a V1 dialog indicating that |extension| has been blocked and call
  // |done_callback| when it completes. Depending on the blocked_action type,
  // the UI of the dialog may differ.
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
  // In V1 flow browser permission dialog or browser blocked dialog is shown.
  // In V2 flow system parent access widget is shown. The widget handles blocked
  // state internally.
  void RequestExtensionApproval(
      const Extension& extension,
      absl::optional<base::WeakPtr<content::WebContents>> contents,
      const gfx::ImageSkia& icon);

  // The V1 dialog pointer is only destroyed when a new dialog is created or the
  // SupervisedUserExtensionsDelegate is destroyed. Therefore there can only be
  // one dialog opened at a time and the last dialog object can have a pretty
  // long lifetime.
  // TODO(b/278874130): Move V1 specific code to its own class for clearer
  // distinction which code is flow specific.
  std::unique_ptr<ParentPermissionDialog> parent_permission_dialog_;

  SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback
      done_callback_;

  std::unique_ptr<ExtensionIconLoader> icon_loader_;

  const raw_ptr<content::BrowserContext> context_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Manages the platform specific V2 approval flow.
  // The extension approvals manager is destroyed when a new ParentAccessDialog
  // is created or this delegate is destroyed.
  std::unique_ptr<ParentAccessExtensionApprovalsManager>
      extension_approvals_manager_;
#endif
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
