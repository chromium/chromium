// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_LIFETIME_MANAGER_H_
#define CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_LIFETIME_MANAGER_H_

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/browsing_data_remover.h"

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace browsing_data {

// The data types values of the BrowsingDataSettings policy.
namespace policy_data_types {
extern const char kBrowsingHistory[];
extern const char kDownloadHistory[];
extern const char kCookiesAndOtherSiteData[];
extern const char kCachedImagesAndFiles[];
extern const char kPasswordSignin[];
extern const char kAutofill[];
extern const char kSiteSettings[];
extern const char kHostedAppData[];
}  // namespace policy_data_types

// The fields of each item defined in the BrowsingDataSettings policy.
namespace policy_fields {
extern const char kTimeToLiveInHours[];
extern const char kDataTypes[];
}  // namespace policy_fields

}  // namespace browsing_data

// Controls the lifetime of some browsing data.
class ChromeBrowsingDataLifetimeManager : public KeyedService {
 public:
  struct ScheduledRemovalSettings {
    uint64_t remove_mask;
    uint64_t origin_type_mask;
    int time_to_live_in_hours;
  };

  explicit ChromeBrowsingDataLifetimeManager(
      content::BrowserContext* browser_context);
  ChromeBrowsingDataLifetimeManager(const ChromeBrowsingDataLifetimeManager&) =
      delete;
  ChromeBrowsingDataLifetimeManager& operator=(
      const ChromeBrowsingDataLifetimeManager&) = delete;
  ~ChromeBrowsingDataLifetimeManager() override;

  // KeyedService:
  void Shutdown() override;

  // Sets the end time of the period for which data must be deleted, for all
  // configurations. If this is |end_time_for_testing| has no value, use the
  // computed end time from each configuration.
  void SetEndTimeForTesting(base::Optional<base::Time> end_time_for_testing) {
    end_time_for_testing_ = std::move(end_time_for_testing);
  }
  void SetBrowsingDataRemoverObserverForTesting(
      content::BrowsingDataRemover::Observer* observer) {
    testing_data_remover_observer_ = observer;
  }

 private:
  // Updates the  scheduled removal settings from the prefs.
  void UpdateScheduledRemovalSettings();
  // Deletes data that needs to be deleted, and schedules the next deletion.
  void StartScheduledBrowsingDataRemoval();

  std::vector<ScheduledRemovalSettings> scheduled_removals_settings_;
  PrefChangeRegistrar pref_change_registrar_;
  Profile* profile_;
  content::BrowsingDataRemover::Observer* testing_data_remover_observer_ =
      nullptr;
  base::Optional<base::Time> end_time_for_testing_;
  base::WeakPtrFactory<ChromeBrowsingDataLifetimeManager> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_LIFETIME_MANAGER_H_
