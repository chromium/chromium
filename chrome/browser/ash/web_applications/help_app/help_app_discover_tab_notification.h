// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_DISCOVER_TAB_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_DISCOVER_TAB_NOTIFICATION_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

constexpr char kShowHelpAppDiscoverTabNotificationId[] =
    "show_help_app_discover_tab_notification";

// Informs the user that the Help app's Discover page has new content and allows
// the user to click on it to navigate directly into the page.
class HelpAppDiscoverTabNotification {
 public:
  explicit HelpAppDiscoverTabNotification(Profile* profile);
  ~HelpAppDiscoverTabNotification();

  HelpAppDiscoverTabNotification(const HelpAppDiscoverTabNotification&) =
      delete;
  HelpAppDiscoverTabNotification& operator=(
      const HelpAppDiscoverTabNotification&) = delete;

  void Show();
  void SetOnClickCallbackForTesting(base::RepeatingCallback<void()> callback);

 private:
  void OnClick(absl::optional<int> button_index);

  Profile* const profile_;
  std::unique_ptr<message_center::Notification> notification_;
  base::RepeatingCallback<void()> onclick_callback_;

  base::WeakPtrFactory<HelpAppDiscoverTabNotification> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_DISCOVER_TAB_NOTIFICATION_H_
