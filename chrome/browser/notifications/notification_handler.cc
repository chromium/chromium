// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_handler.h"

#include "base/functional/callback.h"

NotificationHandler::~NotificationHandler() = default;

void NotificationHandler::OnShow(Profile* profile,
                                 const std::string& notification_id) {}

void NotificationHandler::OnClose(Profile* profile,
                                  const GURL& origin,
                                  const std::string& notification_id,
                                  bool by_user,
                                  base::OnceClosure completed_closure) {
  std::move(completed_closure).Run();
}

void NotificationHandler::OnClick(Profile* profile,
                                  const GURL& origin,
                                  const std::string& notification_id,
                                  const std::optional<int>& action_index,
                                  const std::optional<std::u16string>& reply,
                                  base::OnceClosure completed_closure) {
  std::move(completed_closure).Run();
}

void NotificationHandler::DisableNotifications(Profile* profile,
                                               const GURL& origin) {
  NOTREACHED_IN_MIGRATION();
}

void NotificationHandler::OpenSettings(Profile* profile, const GURL& origin) {
  // Notification types that display a settings button must override this method
  // to handle user interaction with it.
  NOTREACHED_IN_MIGRATION();
}
