// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
    : public extensions::SupervisedUserExtensionsDelegate {
 public:
  SupervisedUserExtensionsDelegateImpl();
  ~SupervisedUserExtensionsDelegateImpl() override;

  // extensions::SupervisedUserExtensionsDelegate overrides
  bool IsChild(content::BrowserContext* context) const override;
  bool IsExtensionAllowedByParent(
      const extensions::Extension& extension,
      content::BrowserContext* context) const override;
  void RequestToAddExtensionOrShowError(
      const extensions::Extension& extension,
      content::BrowserContext* browser_context,
      content::WebContents* web_contents,
      const gfx::ImageSkia& icon,
      ExtensionApprovalDoneCallback extension_approval_callback) override;
  void RequestToEnableExtensionOrShowError(
      const extensions::Extension& extension,
      content::BrowserContext* browser_context,
      content::WebContents* web_contents,
      ExtensionApprovalDoneCallback extension_approval_callback) override;

 private:
  // Returns true if |context| represents a supervised child account who may
  // install extensions with parent permission.
  bool CanInstallExtensions(content::BrowserContext* context) const;

  // Shows a parent permission dialog for |extension| and call |done_callback|
  // when it completes.
  void ShowParentPermissionDialogForExtension(
      const extensions::Extension& extension,
      content::BrowserContext* context,
      content::WebContents* contents,
      const gfx::ImageSkia& icon);

  // Shows a dialog indicating that |extension| has been blocked and call
  // |done_callback| when it completes. Depending on the blocked_action type,
  // the UI of the dialog may differ.
  void ShowInstallBlockedByParentDialogForExtension(
      const extensions::Extension& extension,
      content::WebContents* contents,
      ExtensionInstalledBlockedByParentDialogAction blocked_action);

  // This method is called after all async data are fetched.
  // Since `WebContents` that initiated the request could be destroyed during
  // the async fetch the method cancels the request if `contents` was specified,
  // but is not valid anymore. `contents` is an optional argument because the
  // request can be made from an entry point not associate with `WebContents`.
  void OnExtensionDataLoaded(
      const extensions::Extension& extension,
      content::BrowserContext* context,
      absl::optional<base::WeakPtr<content::WebContents>> contents,
      const gfx::ImageSkia& icon);

  // The V1 dialog pointer is only destroyed when a new dialog is created or the
  // SupervisedUserExtensionsDelegate is destroyed. Therefore there can only be
  // one dialog opened at a time and the last dialog object can have a pretty
  // long lifetime.
  std::unique_ptr<ParentPermissionDialog> parent_permission_dialog_;

  extensions::SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback
      done_callback_;

  std::unique_ptr<ExtensionIconLoader> icon_loader_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
