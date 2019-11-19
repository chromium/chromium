// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request_notification_handler.h"

#include <utility>

#include "base/callback.h"

PermissionRequestNotificationHandler::PermissionRequestNotificationHandler() =
    default;
PermissionRequestNotificationHandler::~PermissionRequestNotificationHandler() =
    default;

void PermissionRequestNotificationHandler::RemoveNotificationDelegate(
    const std::string& notification_id) {
  auto it = notification_delegates_.find(notification_id);
  if (it != notification_delegates_.end())
    notification_delegates_.erase(it);
}

void PermissionRequestNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  Delegate* delegate = GetNotificationDelegate(notification_id);
  if (delegate)
    delegate->Close();
  RemoveNotificationDelegate(notification_id);
  std::move(completed_closure).Run();
}

void PermissionRequestNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    base::OnceClosure completed_closure) {
  // TODO(andypaicu): handle a tap on the body of the notification.
  // If the tap happens on the body of the notification there is no
  // |action_index|.
  if (action_index.has_value()) {
    Delegate* delegate = GetNotificationDelegate(notification_id);
    if (delegate)
      delegate->Click(action_index.value());
  }
  std::move(completed_closure).Run();
}

void PermissionRequestNotificationHandler::AddNotificationDelegate(
    const std::string& notification_id,
    Delegate* notification_delegate) {
  DCHECK(notification_delegates_.find(notification_id) ==
         notification_delegates_.end());
  notification_delegates_[notification_id] = notification_delegate;
}

PermissionRequestNotificationHandler::Delegate*
PermissionRequestNotificationHandler::GetNotificationDelegate(
    const std::string& notification_id) {
  auto it = notification_delegates_.find(notification_id);
  if (it != notification_delegates_.end())
    return it->second;

  return nullptr;
}
