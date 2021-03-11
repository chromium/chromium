// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/muted_notification_handler.h"

#include <utility>

#include "base/callback.h"
#include "base/notreached.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace {
constexpr int kShowActionIndex = 0;
}  // namespace

MutedNotificationHandler::MutedNotificationHandler(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

MutedNotificationHandler::~MutedNotificationHandler() = default;

void MutedNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  if (!action_index)
    delegate_->OnAction(Action::kBodyClick);
  else if (*action_index == kShowActionIndex)
    delegate_->OnAction(Action::kShowClick);
  else
    NOTREACHED();

  std::move(completed_closure).Run();
}

void MutedNotificationHandler::OnClose(Profile* profile,
                                       const GURL& origin,
                                       const std::string& notification_id,
                                       bool by_user,
                                       base::OnceClosure completed_closure) {
  if (by_user)
    delegate_->OnAction(Action::kUserClose);
  std::move(completed_closure).Run();
}

void MutedNotificationHandler::OpenSettings(Profile* profile,
                                            const GURL& origin) {
  NOTREACHED();
}
