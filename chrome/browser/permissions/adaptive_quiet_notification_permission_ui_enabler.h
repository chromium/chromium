// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ADAPTIVE_QUIET_NOTIFICATION_PERMISSION_UI_ENABLER_H_
#define CHROME_BROWSER_PERMISSIONS_ADAPTIVE_QUIET_NOTIFICATION_PERMISSION_UI_ENABLER_H_

#include <memory>
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_util.h"

class PrefChangeRegistrar;
class Profile;

namespace base {
class Clock;
}

// Keeps track of past user interactions with notification permission requests,
// and adaptively enables the quiet permission UX if various heuristics estimate
// the a posteriori probability of the user accepting the subsequent permission
// prompts to be low.
class AdaptiveQuietNotificationPermissionUiEnabler : public KeyedService {
 public:
  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static AdaptiveQuietNotificationPermissionUiEnabler* GetForProfile(
        Profile* profile);

    static AdaptiveQuietNotificationPermissionUiEnabler::Factory* GetInstance();

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;
    content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const override;
  };

  static AdaptiveQuietNotificationPermissionUiEnabler* GetForProfile(
      Profile* profile);

  // Records the outcome of a notification permission prompt, i.e. how the user
  // interacted with it, to be called once a permission request finishes.
  void RecordPermissionPromptOutcome(permissions::PermissionAction action);

  // Delete logs of past user interactions. To be called when clearing
  // browsing data.
  void ClearInteractionHistory(const base::Time& delete_begin,
                               const base::Time& delete_end);

  // The |clock| must outlive this instance.
  void set_clock_for_testing(base::Clock* clock) { clock_ = clock; }

  // Only used for testing.
  void BackfillEnablingMethodIfMissingForTesting() {
    BackfillEnablingMethodIfMissing();
  }

 private:
  explicit AdaptiveQuietNotificationPermissionUiEnabler(Profile* profile);
  ~AdaptiveQuietNotificationPermissionUiEnabler() override;

  // Called when the quiet UI state is updated in preferences.
  void OnQuietUiStateChanged();

  // Retroactively backfills the enabling method, which was not populated
  // before M88.
  void BackfillEnablingMethodIfMissing();

  Profile* profile_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  bool is_enabling_adaptively_ = false;

  // The clock to use as a source of time, materialized so that a mock clock can
  // be injected for tests.
  base::Clock* clock_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AdaptiveQuietNotificationPermissionUiEnabler);
};

#endif  // CHROME_BROWSER_PERMISSIONS_ADAPTIVE_QUIET_NOTIFICATION_PERMISSION_UI_ENABLER_H_
