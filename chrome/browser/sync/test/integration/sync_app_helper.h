// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_APP_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_APP_HELPER_H_

#include <string>

#include "base/memory/singleton.h"
#include "components/sync/model/string_ordinal.h"

class Profile;
class SyncTest;

class SyncAppHelper {
 public:
  // Singleton implementation.
  static SyncAppHelper* GetInstance();

  SyncAppHelper(const SyncAppHelper&) = delete;
  SyncAppHelper& operator=(const SyncAppHelper&) = delete;

  // Initializes the profiles in |test| and registers them with
  // internal data structures.
  void SetupIfNecessary(SyncTest* test);

  // Returns true iff |profile1| and |profile2| have the same apps and
  // they are all in the same state.
  bool AppStatesMatch(Profile* profile1, Profile* profile2);

  // Gets the page ordinal value for the applications with |name| in |profile|.
  syncer::StringOrdinal GetPageOrdinalForApp(Profile* profile,
                                             const std::string& name);

  // Sets a new |page_ordinal| value for the application with |name| in
  // |profile|.
  void SetPageOrdinalForApp(Profile* profile,
                            const std::string& name,
                            const syncer::StringOrdinal& page_ordinal);

  // Gets the app launch ordinal value for the application with |name| in
  // |profile|.
  syncer::StringOrdinal GetAppLaunchOrdinalForApp(Profile* profile,
                                                  const std::string& name);

  // Sets a new |app_launch_ordinal| value for the application with |name| in
  // |profile|.
  void SetAppLaunchOrdinalForApp(
      Profile* profile,
      const std::string& name,
      const syncer::StringOrdinal& app_launch_ordinal);

  // Fix any NTP icon collisions that are currently in |profile|.
  void FixNTPOrdinalCollisions(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<SyncAppHelper>;

  SyncAppHelper();
  ~SyncAppHelper();

  bool setup_completed_ = false;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_APP_HELPER_H_
