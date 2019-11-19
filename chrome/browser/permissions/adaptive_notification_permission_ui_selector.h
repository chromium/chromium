// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ADAPTIVE_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
#define CHROME_BROWSER_PERMISSIONS_ADAPTIVE_NOTIFICATION_PERMISSION_UI_SELECTOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "chrome/browser/permissions/permission_util.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class Clock;
}

// Keeps track of past user interactions with notification permission requests,
// and adaptively enables the quiet permission UX if various heuristics estimate
// the a posteriori probability of the user accepting the subsequent permission
// prompts to be low.
class AdaptiveNotificationPermissionUiSelector : public KeyedService {
 public:
  static AdaptiveNotificationPermissionUiSelector* GetForProfile(
      Profile* profile);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Whether to display subsequent notification permission requests quietly.
  bool ShouldShowQuietUi();

  // Turns off the quiet UI, as per user request.
  void DisableQuietUi();

  // Records the outcome of a notification permission prompt, i.e. how the user
  // interacted with it, to be called once a permission request finishes.
  void RecordPermissionPromptOutcome(PermissionAction action);

  // Delete logs of past user interactions. To be called when clearing
  // browsing data.
  void ClearInteractionHistory(const base::Time& delete_begin,
                               const base::Time& delete_end);

  // The |clock| must outlive this instance.
  void set_clock_for_testing(base::Clock* clock) { clock_ = clock; }

  // Override the result of ShouldShowQuietUi. Only used for testing.
  void set_should_show_quiet_ui_for_testing(bool should_show_quiet_ui) {
    should_show_quiet_ui_ = should_show_quiet_ui;
  }

 private:
  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static AdaptiveNotificationPermissionUiSelector* GetForProfile(
        Profile* profile);

   private:
    static AdaptiveNotificationPermissionUiSelector::Factory* GetInstance();
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;

    content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const override;
  };

  explicit AdaptiveNotificationPermissionUiSelector(Profile* profile);
  ~AdaptiveNotificationPermissionUiSelector() override;

  Profile* profile_;

  // The clock to use as a source of time, materialized so that a mock clock can
  // be injected for tests.
  base::Clock* clock_;

  // An override for the result of ShouldShowQuietUi. Only used in testing.
  base::Optional<bool> should_show_quiet_ui_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AdaptiveNotificationPermissionUiSelector);
};

#endif  // CHROME_BROWSER_PERMISSIONS_ADAPTIVE_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
