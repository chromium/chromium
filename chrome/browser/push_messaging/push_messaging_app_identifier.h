// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_APP_IDENTIFIER_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_APP_IDENTIFIER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "components/push_messaging/app_identifier.h"
#include "url/gurl.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// The integration of push_messaging::AppIdentifier with //chrome.
// This is a static class with only static utility functions. But it's also a
// friend class of push_messaging::AppIdentifier to allow using private
// functions.
// TODO(crbug.com/444713031): Use of the private functions of
// push_messaging::AppIdentifier is less ideal and should be addressed sooner or
// later.
class PushMessagingAppIdentifier final {
 public:
  PushMessagingAppIdentifier() = delete;
  PushMessagingAppIdentifier(const PushMessagingAppIdentifier&) = delete;
  PushMessagingAppIdentifier(PushMessagingAppIdentifier&&) = delete;

  using AppIdentifier = ::push_messaging::AppIdentifier;

  // Register profile-specific prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Looks up an app identifier by app_id. If not found, is_null() will be true.
  static AppIdentifier FindByAppId(Profile* profile, const std::string& app_id);

  // Looks up an app identifier by origin & service worker registration id.
  // If not found, is_null() will be true.
  static AppIdentifier FindByServiceWorker(
      Profile* profile,
      const GURL& origin,
      int64_t service_worker_registration_id);

  // Returns all the AppIdentifiers currently registered for the
  // given |profile|.
  static std::vector<AppIdentifier> GetAll(Profile* profile);

  // Deletes all AppIdentifiers currently registered for the given
  // |profile|.
  static void DeleteAllFromPrefs(Profile* profile);

  // Returns the number of AppIdentifiers currently registered for
  // the given |profile|.
  static size_t GetCount(Profile* profile);

  // Persist this app identifier to prefs.
  static void PersistToPrefs(const AppIdentifier& id, Profile* profile);

  // Delete this app identifier from prefs.
  static void DeleteFromPrefs(const AppIdentifier& id, Profile* profile);
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_APP_IDENTIFIER_H_
