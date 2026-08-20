// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_update_message_delegate_android.h"

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/permissions/android/android_permission_util.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

PermissionUpdateMessageDelegate::PermissionUpdateMessageDelegate(
    content::WebContents* web_contents,
    const std::vector<std::string> required_android_permissions,
    const std::vector<std::string> optional_android_permissions,
    const std::vector<ContentSettingsType> content_settings_types,
    int icon_id,
    int title_id,
    int description_id,
    PermissionUpdatedCallback callback,
    base::OnceCallback<void(PermissionUpdateMessageDelegate*)> delete_callback_)
    : content_settings_types_(content_settings_types),
      delete_callback_(std::move(delete_callback_)) {
  DCHECK(callback);
  callbacks_.push_back(std::move(callback));
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::PERMISSION_UPDATE,
      base::BindOnce(
          &PermissionUpdateMessageDelegate::HandlePrimaryActionCallback,
          base::Unretained(this)),
      base::BindOnce(&PermissionUpdateMessageDelegate::HandleDismissCallback,
                     base::Unretained(this)));

  title_id_ = title_id;
  message_->SetTitle(l10n_util::GetStringUTF16(title_id));
  message_->SetDescription(l10n_util::GetStringUTF16(description_id));
  message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_MESSAGE_UPDATE_PERMISSIONS_BUTTON_TEXT));
  message_->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(icon_id));
  bool enqueued = messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
  permission_update_requester_ = std::make_unique<PermissionUpdateRequester>(
      web_contents, required_android_permissions, optional_android_permissions,
      base::BindOnce(&PermissionUpdateMessageDelegate::OnPermissionResult,
                     base::Unretained(this)));
  // `EnqueueMessage` fails when the WebContents is not attached to a window
  // with a MessageDispatcher (for example, non-tab WebContents like custom
  // overlays or Glic) or when the activity is being recreated/destroyed. In
  // this case, fall back to triggering the Android OS permission request
  // directly.
  if (!enqueued) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PermissionUpdateMessageDelegate::HandlePrimaryActionCallback,
            weak_factory_.GetWeakPtr()));
  }
}

PermissionUpdateMessageDelegate::~PermissionUpdateMessageDelegate() {
  DismissInternal();
}

void PermissionUpdateMessageDelegate::OnPermissionResult(
    bool all_permissions_granted) {
  RunCallbacks(all_permissions_granted);
  permissions::PermissionUmaUtil::RecordMissingPermissionInfobarAction(
      permissions::PermissionAction::GRANTED, content_settings_types_);
  // The callback may destroy `this`.
  // Do not access any member variables after this point.
  std::move(delete_callback_).Run(this);
}

int PermissionUpdateMessageDelegate::GetTitleId() {
  return title_id_;
}

void PermissionUpdateMessageDelegate::AttachAdditionalCallback(
    PermissionUpdatedCallback callback) {
  callbacks_.push_back(std::move(callback));
}

void PermissionUpdateMessageDelegate::HandlePrimaryActionCallback() {
  permission_update_requester_->RequestPermissions();
}

void PermissionUpdateMessageDelegate::HandleDismissCallback(
    messages::DismissReason dismiss_reason) {
  // The message is already being dismissed, prevent DismissInternal() from
  // dismissing again. Do not reset message_ here: this method is called from
  // within MessageWrapper::HandleDismissCallback, and destroying the
  // MessageWrapper at this point would be a use-after-free.
  should_dismiss_internal_ = false;
  // If it is dismissed by clicking on primary action, metrics and callback
  // will be recorded and run in OnPermissionResult.
  // PermissionUpdateRequester::RequestPermissions can invoke its callback
  // synchronously in some cases. In that case, |OnPermissionResult| will be
  // executed before this callback and |callbacks_| will be empty.
  if (dismiss_reason == messages::DismissReason::PRIMARY_ACTION ||
      callbacks_.empty()) {
    return;
  }
  permissions::PermissionUmaUtil::RecordMissingPermissionInfobarAction(
      dismiss_reason == messages::DismissReason::GESTURE
          ? permissions::PermissionAction::DISMISSED
          : permissions::PermissionAction::IGNORED,
      content_settings_types_);
  RunCallbacks(/*all_permissions_granted=*/false);
  // Prevent MessageWrapper destruction during delegate deletion below.
  // Use DeleteSoon to ensure it is destroyed in a later task, after
  // MessageWrapper::HandleDismissCallback has returned.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(message_));
  // This dismiss callback should be executed in the end, because this can
  // destroy the current object.
  std::move(delete_callback_).Run(this);
}

void PermissionUpdateMessageDelegate::DismissInternal() {
  // Skip if the message was already dismissed via HandleDismissCallback.
  if (message_ && should_dismiss_internal_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

void PermissionUpdateMessageDelegate::RunCallbacks(
    bool all_permissions_granted) {
  for (auto& callback : callbacks_) {
    std::move(callback).Run(all_permissions_granted);
  }
  callbacks_.clear();
}
