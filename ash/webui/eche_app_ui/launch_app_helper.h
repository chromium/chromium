// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_LAUNCH_APP_HELPER_H_
#define ASH_WEBUI_ECHE_APP_UI_LAUNCH_APP_HELPER_H_

#include <optional>

#include "ash/webui/eche_app_ui/feature_status.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace gfx {
class Image;
}  //  namespace gfx

namespace ash {

namespace phonehub {
class PhoneHubManager;
}

namespace eche_app {

class AppsLaunchInfoProvider;

// A helper class for launching/closing the app or show a notification.
class LaunchAppHelper {
 public:
  class NotificationInfo {
   public:
    // Enum representing the notification was generated from where.
    enum Category {
      // The notification was generated from native layer,
      kNative,
      // THe notification was generated from webUI,
      kWebUI,
    };

    // Enum representing potential type for the notification.
    enum class NotificationType {
      // Remind users to enable screen lock.
      kScreenLock = 0,
    };

    NotificationInfo(
        Category category,
        absl::variant<NotificationType, mojom::WebNotificationType> type);
    ~NotificationInfo();

    Category category() const { return category_; }
    absl::variant<NotificationType, mojom::WebNotificationType> type() const {
      return type_;
    }

   private:
    Category category_;
    absl::variant<NotificationType, mojom::WebNotificationType> type_;
  };

  using LaunchNotificationFunction =
      base::RepeatingCallback<void(const std::optional<std::u16string>& title,
                                   const std::optional<std::u16string>& message,
                                   std::unique_ptr<NotificationInfo> info)>;
  using CloseNotificationFunction =
      base::RepeatingCallback<void(const std::string& notification_id)>;
  using LaunchEcheAppFunction = base::RepeatingCallback<void(
      const std::optional<int64_t>& notification_id,
      const std::string& package_name,
      const std::u16string& visible_name,
      const std::optional<int64_t>& user_id,
      const gfx::Image& icon,
      const std::u16string& phone_name,
      AppsLaunchInfoProvider* apps_launch_info_provider)>;

  // Enum representing potential reasons why an app is forbidden to launch.
  enum class AppLaunchProhibitedReason {
    // Launching app is allowed.
    kNotProhibited = 0,

    // Launching app is not allowed because it requires the user to enable the
    // screen lock.
    kDisabledByScreenLock = 1,
  };

  LaunchAppHelper(phonehub::PhoneHubManager* phone_hub_manager,
                  LaunchEcheAppFunction launch_eche_app_function,
                  LaunchNotificationFunction launch_notification_function,
                  CloseNotificationFunction close_notification_function);
  virtual ~LaunchAppHelper();

  LaunchAppHelper(const LaunchAppHelper&) = delete;
  LaunchAppHelper& operator=(const LaunchAppHelper&) = delete;

  // Exposed virtual for testing.
  virtual LaunchAppHelper::AppLaunchProhibitedReason
  CheckAppLaunchProhibitedReason(FeatureStatus status) const;

  // Exposed virtual for testing.
  // The notification could be generated from webUI or native layer, for the
  // latter it doesn't carry title and message.
  virtual void ShowNotification(const std::optional<std::u16string>& title,
                                const std::optional<std::u16string>& message,
                                std::unique_ptr<NotificationInfo> info) const;

  // Exposed virtual for testing.
  // Close the notifiication according to id
  virtual void CloseNotification(const std::string& notification_id) const;

  // Exposed virtual for testing.
  // Show the native toast message.
  virtual void ShowToast(const std::u16string& text) const;

  void LaunchEcheApp(std::optional<int64_t> notification_id,
                     const std::string& package_name,
                     const std::u16string& visible_name,
                     const std::optional<int64_t>& user_id,
                     const gfx::Image& icon,
                     const std::u16string& phone_name,
                     AppsLaunchInfoProvider* apps_launch_info_provider);

  const base::flat_set<std::string> GetSessionPackagesLaunchedForTest() const {
    return session_packages_launched_;
  }

 private:
  bool IsScreenLockRequired() const;

  base::flat_set<std::string> session_packages_launched_;
  base::TimeTicks session_packages_last_reset_ = base::TimeTicks();

  raw_ptr<phonehub::PhoneHubManager> phone_hub_manager_;
  LaunchEcheAppFunction launch_eche_app_function_;
  LaunchNotificationFunction launch_notification_function_;
  CloseNotificationFunction close_notification_function_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_LAUNCH_APP_HELPER_H_
