// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_

#include "extensions/browser/supervised_user_extensions_delegate.h"

namespace content {
class BrowserContext;
}

class ParentPermissionDialog;

namespace extensions {

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
  void PromptForParentPermissionOrShowError(
      const extensions::Extension& extension,
      content::BrowserContext* browser_context,
      content::WebContents* web_contents,
      ParentPermissionDialogDoneCallback parent_permission_callback,
      base::OnceClosure error_callback) override;

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
      extensions::SupervisedUserExtensionsDelegate::
          ParentPermissionDialogDoneCallback done_callback);

  // Shows a dialog indicating that |extension| has been blocked and call
  // |done_callback| when it completes.
  void ShowExtensionEnableBlockedByParentDialogForExtension(
      const extensions::Extension& extension,
      content::WebContents* contents,
      base::OnceClosure done_callback);

  std::unique_ptr<ParentPermissionDialog> parent_permission_dialog_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
