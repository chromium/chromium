// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/profile_discard_opt_out_list_helper.h"

#include <algorithm>
#include <vector>

#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_matcher.h"
#include "components/user_prefs/user_prefs.h"
#include "url/gurl.h"

namespace performance_manager::user_tuning {

ProfileDiscardOptOutListHelper::ProfileDiscardOptOutTracker::
    ProfileDiscardOptOutTracker(const std::string& browser_context_id,
                                PrefService* pref_service)
    : browser_context_id_(browser_context_id) {
  pref_change_registrar_.Init(pref_service);

  pref_change_registrar_.Add(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions,
      base::BindRepeating(&ProfileDiscardOptOutListHelper::
                              ProfileDiscardOptOutTracker::OnOptOutListChanged,
                          base::Unretained(this)));
  // Trigger a first update so the list reflects the initial pref value.
  OnOptOutListChanged();
}

ProfileDiscardOptOutListHelper::ProfileDiscardOptOutTracker::
    ~ProfileDiscardOptOutTracker() {
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](std::string browser_context_id,
             performance_manager::Graph* graph) {
            policies::PageDiscardingHelper::GetFromGraph(graph)
                ->ClearNoDiscardPatternsForProfile(browser_context_id);
          },
          browser_context_id_));
}

void ProfileDiscardOptOutListHelper::ProfileDiscardOptOutTracker::
    OnOptOutListChanged() {
  const base::Value::List& value_list =
      pref_change_registrar_.prefs()->GetValueList(
          performance_manager::user_tuning::prefs::kTabDiscardingExceptions);
  std::vector<std::string> patterns;
  patterns.reserve(value_list.size());
  std::transform(value_list.begin(), value_list.end(),
                 std::back_inserter(patterns),
                 [](const base::Value& val) { return val.GetString(); });

  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](std::string browser_context_id, std::vector<std::string> patterns,
             performance_manager::Graph* graph) {
            policies::PageDiscardingHelper::GetFromGraph(graph)
                ->SetNoDiscardPatternsForProfile(browser_context_id, patterns);
          },
          browser_context_id_, std::move(patterns)));
}

ProfileDiscardOptOutListHelper::ProfileDiscardOptOutListHelper() = default;
ProfileDiscardOptOutListHelper::~ProfileDiscardOptOutListHelper() = default;

void ProfileDiscardOptOutListHelper::OnProfileAdded(Profile* profile) {
  discard_opt_out_trackers_.emplace(
      std::piecewise_construct, std::forward_as_tuple(profile->UniqueId()),
      std::forward_as_tuple(profile->UniqueId(), profile->GetPrefs()));
}

void ProfileDiscardOptOutListHelper::OnProfileWillBeRemoved(Profile* profile) {
  auto it = discard_opt_out_trackers_.find(profile->UniqueId());
  DCHECK(it != discard_opt_out_trackers_.end());
  discard_opt_out_trackers_.erase(it);
}

}  // namespace performance_manager::user_tuning
