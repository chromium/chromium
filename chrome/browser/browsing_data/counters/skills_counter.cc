// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/skills_counter.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"

namespace {

bool IsSkillsSyncActive(const syncer::SyncService* sync_service) {
  return sync_service && sync_service->GetActiveDataTypes().Has(syncer::SKILL);
}

}  // namespace

SkillsCounter::SkillsCounter(skills::SkillsService& skills_service,
                             syncer::SyncService* sync_service)
    : skills_service_(skills_service), sync_tracker_(this, sync_service) {}

SkillsCounter::~SkillsCounter() = default;

const char* SkillsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteSkills;
}

void SkillsCounter::OnInitialized() {
  sync_tracker_.OnInitialized(base::BindRepeating(&IsSkillsSyncActive));
}

void SkillsCounter::Count() {
  const base::Time begin_time = GetPeriodStart();
  const base::Time end_time = GetPeriodEnd();
  const ResultInt count = std::ranges::count_if(
      skills_service_->GetSkills(),
      [begin_time, end_time](const std::unique_ptr<skills::Skill>& skill) {
        return skill->last_update_time >= begin_time &&
               skill->last_update_time < end_time;
      });
  ReportResult(
      std::make_unique<SyncResult>(this, count, sync_tracker_.IsSyncActive()));
}
