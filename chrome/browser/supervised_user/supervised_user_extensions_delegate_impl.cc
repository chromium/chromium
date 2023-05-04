// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/extension_icon_loader.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "ui/gfx/image/image_skia.h"

namespace {

void OnParentPermissionDialogComplete(
    extensions::SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback
        delegate_done_callback,
    ParentPermissionDialog::Result result) {
  switch (result) {
    case ParentPermissionDialog::Result::kParentPermissionReceived:
      std::move(delegate_done_callback)
          .Run(extensions::SupervisedUserExtensionsDelegate::
                   ExtensionApprovalResult::kApproved);
      break;
    case ParentPermissionDialog::Result::kParentPermissionCanceled:
      std::move(delegate_done_callback)
          .Run(extensions::SupervisedUserExtensionsDelegate::
                   ExtensionApprovalResult::kCanceled);
      break;
    case ParentPermissionDialog::Result::kParentPermissionFailed:
      std::move(delegate_done_callback)
          .Run(extensions::SupervisedUserExtensionsDelegate::
                   ExtensionApprovalResult::kFailed);
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
  return supervised_user_service->AreExtensionsPermissionsEnabled();
}

bool SupervisedUserExtensionsDelegateImpl::IsExtensionAllowedByParent(
    const extensions::Extension& extension,
    content::BrowserContext* context) const {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserContext(context);
  return supervised_user_service->IsExtensionAllowed(extension);
}

void SupervisedUserExtensionsDelegateImpl::RequestToAddExtensionOrShowError(
    const extensions::Extension& extension,
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const gfx::ImageSkia& icon,
    ExtensionApprovalDoneCallback extension_approval_callback) {
  DCHECK(IsChild(browser_context));
  DCHECK(!IsExtensionAllowedByParent(extension, browser_context));

  done_callback_ = std::move(extension_approval_callback);

  // Supervised users who can install extensions still need parent permission
  // for installation. If the user isn't allowed to install extensions at all,
  // then we will just show a "blocked" dialog.
  if (CanInstallExtensions(browser_context)) {
    ShowParentPermissionDialogForExtension(extension, browser_context,
                                           web_contents, icon);
    return;
  }

  ShowInstallBlockedByParentDialogForExtension(
      extension, web_contents,
      ExtensionInstalledBlockedByParentDialogAction::kAdd);
}

void SupervisedUserExtensionsDelegateImpl::RequestToEnableExtensionOrShowError(
    const extensions::Extension& extension,
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    ExtensionApprovalDoneCallback extension_approval_callback) {
  DCHECK(IsChild(browser_context));
  DCHECK(!IsExtensionAllowedByParent(extension, browser_context));

  done_callback_ = std::move(extension_approval_callback);

  // Supervised users who can install extensions still require parent permission
  // for installation or enablement. If the user isn't allowed to install
  // extensions at all, then we will just show a "blocked" dialog.
  if (CanInstallExtensions(browser_context)) {
    auto icon_callback = base::BindOnce(
        &SupervisedUserExtensionsDelegateImpl::OnExtensionDataLoaded,
        base::Unretained(this), std::cref(extension), browser_context,
        web_contents ? absl::make_optional(web_contents->GetWeakPtr())
                     : absl::nullopt);
    icon_loader_ = std::make_unique<ExtensionIconLoader>();
    icon_loader_->Load(extension, browser_context, std::move(icon_callback));
    return;
  }

  ShowInstallBlockedByParentDialogForExtension(
      extension, web_contents,
      ExtensionInstalledBlockedByParentDialogAction::kEnable);
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
        const gfx::ImageSkia& icon) {
  ParentPermissionDialog::DoneCallback inner_done_callback = base::BindOnce(
      &::OnParentPermissionDialogComplete, std::move(done_callback_));

  gfx::NativeWindow parent_window =
      contents ? contents->GetTopLevelNativeWindow() : nullptr;
  parent_permission_dialog_ =
      ParentPermissionDialog::CreateParentPermissionDialogForExtension(
          Profile::FromBrowserContext(context), parent_window, icon, &extension,
          std::move(inner_done_callback));
  parent_permission_dialog_->ShowDialog();
}

void SupervisedUserExtensionsDelegateImpl::
    ShowInstallBlockedByParentDialogForExtension(
        const extensions::Extension& extension,
        content::WebContents* contents,
        ExtensionInstalledBlockedByParentDialogAction blocked_action) {
  auto block_dialog_callback = base::BindOnce(
      std::move(done_callback_),
      SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kBlocked);
  SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(
      SupervisedUserExtensionsMetricsRecorder::EnablementState::
          kFailedToEnable);
  if (ScopedTestDialogAutoConfirm::GetAutoConfirmValue() !=
      ScopedTestDialogAutoConfirm::NONE) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(block_dialog_callback));
    return;
  }
  ShowExtensionInstallBlockedByParentDialog(
      blocked_action, &extension, contents, std::move(block_dialog_callback));
}

void SupervisedUserExtensionsDelegateImpl::OnExtensionDataLoaded(
    const extensions::Extension& extension,
    content::BrowserContext* context,
    absl::optional<base::WeakPtr<content::WebContents>> contents,
    const gfx::ImageSkia& icon) {
  // Treat the request as canceled if web contents that the request originated
  // in was destroyed (the web contents was originally passed, but weak ptr is
  // not valid anymore).
  content::WebContents* web_contents = nullptr;
  if (contents) {
    base::WeakPtr<content::WebContents> contents_weak_ptr = contents.value();
    if (!contents_weak_ptr) {
      std::move(done_callback_)
          .Run(extensions::SupervisedUserExtensionsDelegate::
                   ExtensionApprovalResult::kCanceled);
      return;
    }
    web_contents = contents_weak_ptr.get();
  }
  ShowParentPermissionDialogForExtension(extension, context, web_contents,
                                         icon);
}

}  // namespace extensions
