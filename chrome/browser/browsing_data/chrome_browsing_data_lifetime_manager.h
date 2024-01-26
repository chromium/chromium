// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_LIFETIME_MANAGER_H_
#define CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_LIFETIME_MANAGER_H_

#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/base/user_selectable_type.h"
#include "content/public/browser/browsing_data_remover.h"

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace browsing_data {

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
  void SetEndTimeForTesting(std::optional<base::Time> end_time_for_testing) {
    end_time_for_testing_ = std::move(end_time_for_testing);
  }
  void SetBrowsingDataRemoverObserverForTesting(
      content::BrowsingDataRemover::Observer* observer) {
    testing_data_remover_observer_ = observer;
  }

  // Deletes all browsing data specified by the ClearBrowsingDataOnExitList
  // policy for on the record profiles. This blocks shutdown until the required
  // data is deleted. This will be called when the last browser window of a
  // Profile is closed, then browsing data will be cleared according to the
  // ClearBrowsingDataOnExitList policy. The browser and profile will have to be
  // kept alive until the data is deleted. In case the browsing data clearing
  // does not end properly, this may be called at the next startup in order to
  // cleanup any remaining data. The browser and profile must be kept from
  // shutting down while this is running because an early deletion of the
  // profile will abort the browsing data clearing but still notify us that the
  // data was cleared, even if it was not. If |keep_browser_alive|, the browser
  // will be kept alive until the deletion is completed. This should be done if
  // this function is called as part of a shutdown, but not as part of a cleanup
  // at startup.
  void ClearBrowsingDataForOnExitPolicy(bool keep_browser_alive);

 private:
  // Updates the  scheduled removal settings from the prefs.
  void UpdateScheduledRemovalSettings();
  // Deletes data that needs to be deleted, and schedules the next deletion.
  void StartScheduledBrowsingDataRemoval();

  std::vector<ScheduledRemovalSettings> scheduled_removals_settings_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::BrowsingDataRemover::Observer, DanglingUntriaged>
      testing_data_remover_observer_ = nullptr;
  std::optional<base::Time> end_time_for_testing_;
  base::WeakPtrFactory<ChromeBrowsingDataLifetimeManager> weak_ptr_factory_{
      this};

  // Checks that the conditions needed to clear the browsing data types are
  // satisfied. 'sync_types' are checked if neither sync nor
  // browser sign in are disabled.
  bool IsConditionSatisfiedForBrowsingDataRemoval(
      const syncer::UserSelectableTypeSet sync_types);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_LIFETIME_MANAGER_H_
