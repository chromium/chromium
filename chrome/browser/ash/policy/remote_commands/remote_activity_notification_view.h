// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_REMOTE_ACTIVITY_NOTIFICATION_VIEW_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_REMOTE_ACTIVITY_NOTIFICATION_VIEW_H_

#include "base/functional/callback_forward.h"
#include "ui/views/view.h"

namespace policy {

// View that displays a notification after a remote admin accessed the device
// via CRD and there was no user present at the device.
class RemoteActivityNotificationView : public views::View {
 public:
  METADATA_HEADER(RemoteActivityNotificationView);
  explicit RemoteActivityNotificationView(
      base::RepeatingClosure on_button_pressed);
};

BEGIN_VIEW_BUILDER(, RemoteActivityNotificationView, views::View)
END_VIEW_BUILDER

}  // namespace policy

DEFINE_VIEW_BUILDER(, policy::RemoteActivityNotificationView)

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_REMOTE_ACTIVITY_NOTIFICATION_VIEW_H_
