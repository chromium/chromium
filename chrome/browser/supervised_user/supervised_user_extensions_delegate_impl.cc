// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"

namespace {

void OnParentPermissionDialogComplete(
    extensions::SupervisedUserExtensionsDelegate::
        ParentPermissionDialogDoneCallback delegate_done_callback,
    ParentPermissionDialog::Result result) {
  switch (result) {
    case ParentPermissionDialog::Result::kParentPermissionReceived:
      std::move(delegate_done_callback)
          .Run(extensions::SupervisedUserExtensionsDelegate::
                   ParentPermissionDialogResult::kParentPermissionReceived);
      break;
    case ParentPermissionDialog::Result::kParentPermissionCanceled:
      std::move(delegate_done_callback)
          .Run(extensions::SupervisedUserExtensionsDelegate::
                   ParentPermissionDialogResult::kParentPermissionCanceled);
      break;
    case ParentPermissionDialog::Result::kParentPermissionFailed:
      std::move(delegate_done_callback)
          .Run(extensions::SupervisedUserExtensionsDelegate::
                   ParentPermissionDialogResult::kParentPermissionFailed);
      break;
  }
}

}  // namespace

namespace extensions {

SupervisedUserExtensionsDelegateImpl::SupervisedUserExtensionsDelegateImpl() =
    default;

SupervisedUserExtensionsDelegateImpl::~SupervisedUserExtensionsDelegateImpl() =
    default;

bool SupervisedUserExtensionsDelegateImpl::IsChild(
    content::BrowserContext* context) const {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserContext(context);
  return supervised_user_service->IsChild();
}

bool SupervisedUserExtensionsDelegateImpl::IsExtensionAllowedByParent(
    const extensions::Extension& extension,
    content::BrowserContext* context) const {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserContext(context);
  return supervised_user_service->IsExtensionAllowed(extension);
}

void SupervisedUserExtensionsDelegateImpl::PromptForParentPermissionOrShowError(
    const extensions::Extension& extension,
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    ParentPermissionDialogDoneCallback parent_permission_callback,
    base::OnceClosure error_callback) {
  DCHECK(IsChild(browser_context));
  DCHECK(!IsExtensionAllowedByParent(extension, browser_context));

  // Supervised users who can install extensions still require parent permission
  // for installation or enablement. If the user isn't allowed to install
  // extensions at all, then we will just show a "blocked" dialog.
  if (CanInstallExtensions(browser_context)) {
    ShowParentPermissionDialogForExtension(
        extension, browser_context, web_contents,
        std::move(parent_permission_callback));
  } else {
    ShowExtensionEnableBlockedByParentDialogForExtension(
        extension, web_contents, std::move(error_callback));
  }
}

bool SupervisedUserExtensionsDelegateImpl::CanInstallExtensions(
    content::BrowserContext* context) const {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserContext(context);
  return supervised_user_service->CanInstallExtensions();
}

void SupervisedUserExtensionsDelegateImpl::
    ShowParentPermissionDialogForExtension(
        const extensions::Extension& extension,
        content::BrowserContext* context,
        content::WebContents* contents,
        ParentPermissionDialogDoneCallback done_callback) {
  ParentPermissionDialog::DoneCallback inner_done_callback = base::BindOnce(
      &::OnParentPermissionDialogComplete, std::move(done_callback));

  gfx::NativeWindow parent_window =
      contents ? contents->GetTopLevelNativeWindow() : nullptr;
  parent_permission_dialog_ =
      ParentPermissionDialog::CreateParentPermissionDialogForExtension(
          Profile::FromBrowserContext(context), parent_window, gfx::ImageSkia(),
          &extension, std::move(inner_done_callback));
  parent_permission_dialog_->ShowDialog();
}

void SupervisedUserExtensionsDelegateImpl::
    ShowExtensionEnableBlockedByParentDialogForExtension(
        const extensions::Extension& extension,
        content::WebContents* contents,
        base::OnceClosure done_callback) {
  SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(
      SupervisedUserExtensionsMetricsRecorder::EnablementState::
          kFailedToEnable);
  if (ScopedTestDialogAutoConfirm::GetAutoConfirmValue() !=
      ScopedTestDialogAutoConfirm::NONE) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(done_callback));
    return;
  }
  ShowExtensionInstallBlockedByParentDialog(
      ExtensionInstalledBlockedByParentDialogAction::kEnable, &extension,
      contents, std::move(done_callback));
}

}  // namespace extensions
