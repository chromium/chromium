// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_DECIDER_H_
#define CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_DECIDER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class OomInterventionDeciderTest;
class PrefService;

// This class contains triggering and opting-out logic for OOM intervention.
// Opt-out logic:
// - If user declined intervention, don't trigger it until OOM is observed on
//   the same site.
// - If user declined intervention again even after OOM is observed on the site,
//   never trigger intervention on the site.
// - If len(blocklist) > kMaxBlocklistSize, the user is permanently opted out.
//
// An instance of this class is associated with BrowserContext when the
// BrowserContext isn't incognito. You can obtain it via GetForBrowserContext().
// GetForBrowserContext() will return nullptr when in incognito mode.
class OomInterventionDecider : public base::SupportsUserData::Data {
 public:
  // This delegate is a testing seam.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual bool WasLastShutdownClean() = 0;
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static OomInterventionDecider* GetForBrowserContext(
      content::BrowserContext* context);

  OomInterventionDecider(const OomInterventionDecider&) = delete;
  OomInterventionDecider& operator=(const OomInterventionDecider&) = delete;

  ~OomInterventionDecider() override;

  bool CanTriggerIntervention(const std::string& host) const;

  void OnInterventionDeclined(const std::string& host);
  void OnOomDetected(const std::string& host);

  void ClearData();

 private:
  OomInterventionDecider(std::unique_ptr<Delegate> delegate,
                         PrefService* prefs);

  friend class OomInterventionDeciderTest;
  FRIEND_TEST_ALL_PREFIXES(OomInterventionDeciderTest, OptOutSingleHost);
  FRIEND_TEST_ALL_PREFIXES(OomInterventionDeciderTest, ParmanentlyOptOut);
  FRIEND_TEST_ALL_PREFIXES(OomInterventionDeciderTest, WasLastShutdownClean);

  // These constants are declared here for testing.
  static const size_t kMaxListSize;
  static const size_t kMaxBlocklistSize;

  // Called when |prefs_| is ready to use. When the last shutdown wasn't clean,
  // this method adds the last entry of the declined list to the OOM detected
  // list, assuming that the site caused the crash and the crash was due to
  // OOM.
  void OnPrefInitialized(bool success);

  bool IsOptedOut(const std::string& host) const;

  bool IsInList(const char* list_name, const std::string& host) const;
  void AddToList(const char* list_name, const std::string& host);

  std::unique_ptr<Delegate> delegate_;
  raw_ptr<PrefService> prefs_;
};

#endif  // CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_DECIDER_H_
