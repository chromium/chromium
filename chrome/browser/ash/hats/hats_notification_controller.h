// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_HATS_HATS_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_HATS_HATS_NOTIFICATION_CONTROLLER_H_

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace message_center {
class Notification;
}  // namespace message_center

class Profile;
class NetworkState;

namespace ash {

struct HatsConfig;

// TODO(jackshira): Extract non-notification specific code into a manager class.
// Happiness tracking survey (HaTS) notification controller is responsible for
// managing the HaTS notification that is displayed to the user.
// This class lives on the UI thread.
class HatsNotificationController : public message_center::NotificationDelegate,
                                   public NetworkStateHandlerObserver,
                                   public ProfileObserver {
 public:
  static const char kNotificationId[];

  // Minimum amount of time before the notification is displayed again after a
  // user has interacted with it.
  static constexpr base::TimeDelta kHatsThreshold = base::Days(60);

  // The threshold for a Googler is less.
  static constexpr base::TimeDelta kHatsGooglerThreshold = base::Days(30);

  // Prioritized HaTS has much shorter threshold.
  static constexpr base::TimeDelta kPrioritizedHatsThreshold = base::Days(10);

  // There are multiple pool/quota of cooldowns: normal and prioritized,
  // when user was selected for one pool, there need to be another cooldown
  // to ensure the user would not be selected for the other pool immediately
  // after.
  static constexpr base::TimeDelta kMinimumHatsThreshold = base::Days(1);

  // HaTS threshold should be configured correctly.
  static_assert(kHatsThreshold > kPrioritizedHatsThreshold);
  static_assert(kHatsThreshold > kHatsGooglerThreshold);
  static_assert(kPrioritizedHatsThreshold > kMinimumHatsThreshold);
  static_assert(kHatsGooglerThreshold > kMinimumHatsThreshold);

  HatsNotificationController(
      Profile* profile,
      const HatsConfig& config,
      const base::flat_map<std::string, std::string>& product_specific_data,
      const std::u16string title,
      const std::u16string body);

  // |product_specific_data| is meant to allow attaching extra runtime data that
  // is specific to the survey, e.g. a survey about the log-in experience might
  // include the last used authentication method.
  HatsNotificationController(
      Profile* profile,
      const HatsConfig& config,
      const base::flat_map<std::string, std::string>& product_specific_data);

  HatsNotificationController(Profile* profile, const HatsConfig& config);

  HatsNotificationController(const HatsNotificationController&) = delete;
  HatsNotificationController& operator=(const HatsNotificationController&) =
      delete;

  // Returns true if the survey needs to be displayed for the given |profile|.
  static bool ShouldShowSurveyToProfile(Profile* profile,
                                        const HatsConfig& config);

 private:
  friend class HatsNotificationControllerTest;
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           GetFormattedSiteContext);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           NewDevice_ShouldNotShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           OldDevice_ShouldShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           NoInternet_DoNotShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           InternetConnected_ShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           DismissNotification_ShouldUpdatePref);
  FRIEND_TEST_ALL_PREFIXES(
      HatsNotificationControllerTest,
      Disconnected_RemoveNotification_Connected_AddNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           DismissNotification_PrioritizedShouldUpdatePref);

  ~HatsNotificationController() override;

  enum class HatsState {
    kDeviceSelected = 0,         // Device was selected in roll of dice.
    kSurveyShownRecently = 1,    // A survey was shown recently on device.
    kNewDevice = 2,              // Device is too new to show the survey.
    kNotificationDisplayed = 3,  // Pop up for survey was presented to user.
    kNotificationDismissed = 4,  // Notification was dismissed by user.
    kNotificationClicked = 5,    // User clicked on notification to open the
                                 // survey.

    kMaxValue = kNotificationClicked
  };

  void Initialize(bool is_new_device);

  // NotificationDelegate overrides:
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

  // NetworkStateHandlerObserver override:
  void PortalStateChanged(const NetworkState* default_network,
                          NetworkState::PortalState portal_state) override;
  void OnShuttingDown() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Must be run on a blocking thread pool.
  // Gathers the browser version info, firmware info and platform info and
  // returns them in a single encoded string, in the format
  // "<key>=<value>&<key>=<value>&<key>=<value>" where the keys and values are
  // url-escaped. Any key-value pairs in |product_specific_data| are also
  // encoded and appended to the string, unless the keys collide with existing
  // device info keys.
  static std::string GetFormattedSiteContext(
      const std::string& user_locale,
      const base::flat_map<std::string, std::string>& product_specific_data);
  void UpdateLastInteractionTime();
  void UpdateLastSurveyInteractionTime();
  void ShowDialog(const std::string& site_context);

  raw_ptr<Profile> profile_;
  const raw_ref<const HatsConfig> hats_config_;
  base::flat_map<std::string, std::string> product_specific_data_;
  std::unique_ptr<message_center::Notification> notification_;
  const std::u16string title_;
  const std::u16string body_;

  HatsState state_ = HatsState::kDeviceSelected;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<HatsNotificationController> weak_pointer_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_HATS_HATS_NOTIFICATION_CONTROLLER_H_
