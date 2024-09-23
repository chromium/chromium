// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/muted_notification_handler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"

MutedNotificationHandler::MutedNotificationHandler(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

MutedNotificationHandler::~MutedNotificationHandler() = default;

void MutedNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  base::ScopedClosureRunner runner(std::move(completed_closure));

  if (!action_index) {
    delegate_->OnAction(Action::kBodyClick);
    return;
  }

  // Indices of actions on the "Notifications Muted" notification must match the
  // order in which we add the action buttons to the notification in
  // ScreenCaptureNotificationBlocker::DisplayMuteNotification().

  if (base::FeatureList::IsEnabled(features::kMuteNotificationSnoozeAction)) {
    if (*action_index == 0)
      delegate_->OnAction(Action::kSnoozeClick);
    else if (*action_index == 1)
      delegate_->OnAction(Action::kShowClick);
    return;
  }

  if (*action_index == 0)
    delegate_->OnAction(Action::kShowClick);
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
  NOTREACHED_IN_MIGRATION();
}
