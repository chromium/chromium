// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_SKILLS_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_SKILLS_COUNTER_H_

#include "base/memory/raw_ref.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/sync_tracker.h"

namespace skills {
class SkillsService;
}  // namespace skills

namespace syncer {
class SyncService;
}  // namespace syncer

// A BrowsingDataCounter that counts the number of user skills that were last
// modified within the selected time period. The reported result is a
// `SyncResult` that also indicates whether skills are currently being synced,
// in which case the deletion will be propagated to the user's account.
class SkillsCounter : public browsing_data::BrowsingDataCounter {
 public:
  // `sync_service` may be null.
  SkillsCounter(skills::SkillsService& skills_service,
                syncer::SyncService* sync_service);

  SkillsCounter(const SkillsCounter&) = delete;
  SkillsCounter& operator=(const SkillsCounter&) = delete;

  ~SkillsCounter() override;

  const char* GetPrefName() const override;

 private:
  void OnInitialized() override;
  void Count() override;

  const raw_ref<skills::SkillsService> skills_service_;
  browsing_data::SyncTracker sync_tracker_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_SKILLS_COUNTER_H_
