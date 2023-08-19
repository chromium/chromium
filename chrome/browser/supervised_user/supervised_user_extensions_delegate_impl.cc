// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/extension_icon_loader.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_manager.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/features.h"
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

SupervisedUserExtensionsDelegateImpl::SupervisedUserExtensionsDelegateImpl(
    content::BrowserContext* browser_context)
    : context_(browser_context), extensions_manager_(context_) {
  CHECK(context_);
}

SupervisedUserExtensionsDelegateImpl::~SupervisedUserExtensionsDelegateImpl() =
    default;

void SupervisedUserExtensionsDelegateImpl::
    UpdateManagementPolicyRegistration() {
  extensions_manager_.UpdateManagementPolicyRegistration();
}

bool SupervisedUserExtensionsDelegateImpl::IsChild() const {
  return SupervisedUserServiceFactory::GetForBrowserContext(context_)
      ->AreExtensionsPermissionsEnabled();
}

bool SupervisedUserExtensionsDelegateImpl::IsExtensionAllowedByParent(
    const Extension& extension) const {
  return extensions_manager_.IsExtensionAllowed(extension);
}

void SupervisedUserExtensionsDelegateImpl::RequestToAddExtensionOrShowError(
    const Extension& extension,
    content::WebContents* web_contents,
    const gfx::ImageSkia& icon,
    ExtensionApprovalDoneCallback extension_approval_callback) {
  DCHECK(IsChild());
  DCHECK(!IsExtensionAllowedByParent(extension));

  done_callback_ = std::move(extension_approval_callback);
  RequestExtensionApproval(extension, web_contents->GetWeakPtr(), icon);
}

void SupervisedUserExtensionsDelegateImpl::RequestToEnableExtensionOrShowError(
    const Extension& extension,
    content::WebContents* web_contents,
    ExtensionApprovalDoneCallback extension_approval_callback) {
  CHECK(IsChild());
  CHECK(!IsExtensionAllowedByParent(extension));

  done_callback_ = std::move(extension_approval_callback);

  // Fetch icon from local resources and then show enable flow.
  auto icon_callback = base::BindOnce(
      &SupervisedUserExtensionsDelegateImpl::RequestExtensionApproval,
      base::Unretained(this), std::cref(extension),
      web_contents ? absl::make_optional(web_contents->GetWeakPtr())
                   : absl::nullopt);
  icon_loader_ = std::make_unique<ExtensionIconLoader>();
  icon_loader_->Load(extension, context_, std::move(icon_callback));
}

bool SupervisedUserExtensionsDelegateImpl::CanInstallExtensions() const {
  return extensions_manager_.CanInstallExtensions();
}

void SupervisedUserExtensionsDelegateImpl::AddExtensionApproval(
    const extensions::Extension& extension) {
  extensions_manager_.AddExtensionApproval(extension);
}

void SupervisedUserExtensionsDelegateImpl::RemoveExtensionApproval(
    const extensions::Extension& extension) {
  extensions_manager_.RemoveExtensionApproval(extension);
}

void SupervisedUserExtensionsDelegateImpl::RecordExtensionEnablementUmaMetrics(
    bool enabled) const {
  extensions_manager_.RecordExtensionEnablementUmaMetrics(enabled);
}

void SupervisedUserExtensionsDelegateImpl::
    ShowParentPermissionDialogForExtension(const Extension& extension,
                                           content::WebContents* contents,
                                           const gfx::ImageSkia& icon) {
  ParentPermissionDialog::DoneCallback inner_done_callback = base::BindOnce(
      &::OnParentPermissionDialogComplete, std::move(done_callback_));

  gfx::NativeWindow parent_window =
      contents ? contents->GetTopLevelNativeWindow() : nullptr;
  parent_permission_dialog_ =
      ParentPermissionDialog::CreateParentPermissionDialogForExtension(
          Profile::FromBrowserContext(context_), parent_window, icon,
          &extension, std::move(inner_done_callback));
  parent_permission_dialog_->ShowDialog();
}

void SupervisedUserExtensionsDelegateImpl::
    ShowInstallBlockedByParentDialogForExtension(
        const Extension& extension,
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
      ExtensionInstalledBlockedByParentDialogAction::kEnable, &extension,
      contents, std::move(block_dialog_callback));
}

void SupervisedUserExtensionsDelegateImpl::RequestExtensionApproval(
    const Extension& extension,
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

#if BUILDFLAG(IS_CHROMEOS)
  if (ParentAccessExtensionApprovalsManager::ShouldShowExtensionApprovalsV2()) {
    // Parent Access Dialog handles blocked use case in V2.
    extension_approvals_manager_ =
        std::make_unique<ParentAccessExtensionApprovalsManager>();
    extension_approvals_manager_->ShowParentAccessDialog(
        extension, context_, icon,
        CanInstallExtensions()
            ? ParentAccessExtensionApprovalsManager::ExtensionInstallMode::
                  kInstallationPermitted
            : ParentAccessExtensionApprovalsManager::ExtensionInstallMode::
                  kInstallationDenied,
        std::move(done_callback_));
    return;
  }
#endif

  if (CanInstallExtensions()) {
    ShowParentPermissionDialogForExtension(extension, web_contents, icon);
    return;
  }

  ShowInstallBlockedByParentDialogForExtension(
      extension, web_contents,
      ExtensionInstalledBlockedByParentDialogAction::kEnable);
}

}  // namespace extensions
