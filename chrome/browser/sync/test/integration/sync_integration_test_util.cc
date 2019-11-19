// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/test/integration/themes_helper.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/sync/driver/profile_sync_service.h"
#include "content/public/test/test_utils.h"

void SetCustomTheme(Profile* profile, int theme_index) {
  themes_helper::UseCustomTheme(profile, theme_index);
  content::WindowedNotificationObserver theme_change_observer(
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile)));
  theme_change_observer.Wait();
}

ServerCountMatchStatusChecker::ServerCountMatchStatusChecker(
    syncer::ModelType type,
    size_t count)
    : type_(type), count_(count) {}

bool ServerCountMatchStatusChecker::IsExitConditionSatisfied(std::ostream* os) {
  size_t actual_count = fake_server()->GetSyncEntitiesByModelType(type_).size();
  *os << "Waiting for fake server entity count " << actual_count
      << " to match expected count " << count_ << " for type "
      << ModelTypeToString(type_);
  return count_ == actual_count;
}

PassphraseRequiredChecker::PassphraseRequiredChecker(
    syncer::ProfileSyncService* service)
    : SingleClientStatusChangeChecker(service) {}

bool PassphraseRequiredChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Passhrase Required";
  return service()->GetUserSettings()->IsPassphraseRequired();
}

PassphraseAcceptedChecker::PassphraseAcceptedChecker(
    syncer::ProfileSyncService* service)
    : SingleClientStatusChangeChecker(service) {}

bool PassphraseAcceptedChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Passhrase Accepted";
  return !service()->GetUserSettings()->IsPassphraseRequired() &&
         service()->GetUserSettings()->IsUsingSecondaryPassphrase();
}
