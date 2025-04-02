// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_PREF_CHANGE_HANDLER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_PREF_CHANGE_HANDLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"
#include "chrome/browser/safe_browsing/tailored_security/consented_message_android.h"
#include "chrome/browser/safe_browsing/tailored_security/unconsented_message_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#endif

class Profile;

namespace safe_browsing {

// Handles showing the appropriate toast or modal when the Safe Browsing
// protection setting changes. This class is not thread-safe.
#if BUILDFLAG(IS_ANDROID)
class SafeBrowsingPrefChangeHandler : public TabModelObserver,
                                      public TabModelListObserver {
#else
class SafeBrowsingPrefChangeHandler {
#endif
 public:
  explicit SafeBrowsingPrefChangeHandler(Profile* profile);
#if BUILDFLAG(IS_ANDROID)
  ~SafeBrowsingPrefChangeHandler() override;
#else
  virtual ~SafeBrowsingPrefChangeHandler();
#endif

  // The amount of time to wait after construction before checking if a retry is
  // needed.
  static constexpr const base::TimeDelta kRetryAttemptStartupDelay =
      base::Minutes(2);

  // The amount of time to wait between retry attempts.
  static constexpr const base::TimeDelta kRetryNextAttemptDelay = base::Days(1);

  // Length of time that the retry mechanism will wait before running. This
  // delay is used for the case where the safe browsing pref change handler
  // can't tell if it succeeded in the past.
  static constexpr const base::TimeDelta kWaitingPeriodInterval = base::Days(2);

  // Handles notifying the user when necessary. The type of notification shown
  // depends on the platform and whether the user is currently on the security
  // settings page. Virtual for tests.
  virtual void MaybeShowEnhancedProtectionSettingChangeNotification();

 private:
  // Member variable to store the Profile*.
  raw_ptr<Profile> profile_;
#if BUILDFLAG(IS_ANDROID)
  // Called when the consented modal is dismissed.
  void ConsentedMessageDismissed();

  friend class SafeBrowsingPrefChangeHandlerAndroidTest;

  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingPrefChangeHandlerAndroidTest,
                           AddAndRemoveTabModelListObserver);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingPrefChangeHandlerAndroidTest,
                           AddAndRemoveTabModelObserver);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingPrefChangeHandlerAndroidTest,
                           AddTabModelObserver_NoMatchingProfile);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingPrefChangeHandlerAndroidTest,
                           RegisterObserver);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingPrefChangeHandlerAndroidTest, DidAddTab);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingPrefChangeHandlerAndroidTest,
                           DidAddTab_NullTab);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingPrefChangeHandlerAndroidTest,
                           OnTabModelAddedAndRemoved);

  // Functions used for testing.
  // Sets the TabModel for testing purposes.
  void SetTabModelForTesting(TabModel* tab_model);

  // Checks if the handler is currently observing the TabModelList.
  bool IsObservingTabModelListForTesting() const;

  // Checks if the handler is currently observing a TabModel.
  bool IsObservingTabModelForTesting() const;

  // Updates the retry state and tris to show notification when needed.
  void RetryStateCallback();

  // Registers this as an observer on the TabModelList and, if possible, on a
  // TabModel.
  void RegisterObserver();
  void AddTabModelListObserver();
  void AddTabModelObserver();
  void RemoveTabModelListObserver();
  void RemoveTabModelObserver();

  // TabModelObserver::
  void DidAddTab(TabAndroid* tab, TabModel::TabLaunchType type) override;
  // TabModelListObserver::
  void OnTabModelAdded(TabModel* tab_model) override;
  void OnTabModelRemoved(TabModel* tab_model) override;

  // This tab model is used for the observer based retry mechanism.
  // We can't depend on this being set as a tab can be deleted at
  // any time.
  raw_ptr<TabModel> observed_tab_model_ = nullptr;
  bool observing_tab_model_list_ = false;

  // The retry handler used to manage retry logic.
  std::unique_ptr<MessageRetryHandler> retry_handler_;

  // The modal that is shown to the user.
  std::unique_ptr<TailoredSecurityConsentedModalAndroid> message_;

  base::WeakPtrFactory<SafeBrowsingPrefChangeHandler> weak_ptr_factory_{this};
#endif
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_PREF_CHANGE_HANDLER_H_
