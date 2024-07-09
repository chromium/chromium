// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/profile_discard_opt_out_list_helper.h"

#include <vector>

#include "base/feature_list.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
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
namespace {

class ProfileDiscardOptOutListHelperDelegateImpl
    : public ProfileDiscardOptOutListHelper::Delegate {
 public:
  ~ProfileDiscardOptOutListHelperDelegateImpl() override = default;

  void ClearPatterns(const std::string& browser_context_id) override {
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            [](std::string browser_context_id,
               performance_manager::Graph* graph) {
              policies::PageDiscardingHelper::GetFromGraph(graph)
                  ->ClearNoDiscardPatternsForProfile(browser_context_id);
            },
            browser_context_id));
  }

  void SetPatterns(const std::string& browser_context_id,
                   const std::vector<std::string>& patterns) override {
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindOnce(
                       [](std::string browser_context_id,
                          std::vector<std::string> patterns,
                          performance_manager::Graph* graph) {
                         policies::PageDiscardingHelper::GetFromGraph(graph)
                             ->SetNoDiscardPatternsForProfile(
                                 browser_context_id, patterns);
                       },
                       browser_context_id, std::move(patterns)));
  }
};

}  // namespace

ProfileDiscardOptOutListHelper::ProfileDiscardOptOutTracker::
    ProfileDiscardOptOutTracker(const std::string& browser_context_id,
                                PrefService* pref_service,
                                Delegate* delegate)
    : browser_context_id_(browser_context_id), delegate_(delegate) {
  pref_change_registrar_.Init(pref_service);

  pref_change_registrar_.Add(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptionsWithTime,
      base::BindRepeating(&ProfileDiscardOptOutListHelper::
                              ProfileDiscardOptOutTracker::OnOptOutListChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      performance_manager::user_tuning::prefs::kManagedTabDiscardingExceptions,
      base::BindRepeating(&ProfileDiscardOptOutListHelper::
                              ProfileDiscardOptOutTracker::OnOptOutListChanged,
                          base::Unretained(this)));
  // Trigger a first update so the list reflects the initial pref value.
  OnOptOutListChanged();
}

ProfileDiscardOptOutListHelper::ProfileDiscardOptOutTracker::
    ~ProfileDiscardOptOutTracker() {
  delegate_->ClearPatterns(browser_context_id_);
}

void ProfileDiscardOptOutListHelper::ProfileDiscardOptOutTracker::
    OnOptOutListChanged() {
  const base::Value::Dict& user_value_map =
      pref_change_registrar_.prefs()->GetDict(
          performance_manager::user_tuning::prefs::
              kTabDiscardingExceptionsWithTime);
  const base::Value::List& managed_value_list =
      pref_change_registrar_.prefs()->GetList(
          performance_manager::user_tuning::prefs::
              kManagedTabDiscardingExceptions);

  std::vector<std::string> patterns;
  patterns.reserve(user_value_map.size() + managed_value_list.size());

  // Merge the two lists so that the PageDiscardingHelper only sees a single
  // list of patterns to exclude from discarding.
  base::ranges::transform(
      user_value_map.begin(), user_value_map.end(),
      std::back_inserter(patterns),
      [](const auto& user_value) { return user_value.first; });
  base::ranges::transform(
      managed_value_list, std::back_inserter(patterns),
      [](const auto& managed_value) { return managed_value.GetString(); });

  delegate_->SetPatterns(browser_context_id_, patterns);
}

ProfileDiscardOptOutListHelper::ProfileDiscardOptOutListHelper(
    std::unique_ptr<Delegate> delegate)
    : delegate_(delegate ? std::move(delegate)
                         : std::make_unique<
                               ProfileDiscardOptOutListHelperDelegateImpl>()) {}

ProfileDiscardOptOutListHelper::~ProfileDiscardOptOutListHelper() = default;

void ProfileDiscardOptOutListHelper::OnProfileAdded(Profile* profile) {
  OnProfileAddedImpl(profile->UniqueId(), profile->GetPrefs());
}

void ProfileDiscardOptOutListHelper::OnProfileWillBeRemoved(Profile* profile) {
  OnProfileWillBeRemovedImpl(profile->UniqueId());
}

void ProfileDiscardOptOutListHelper::OnProfileAddedImpl(
    const std::string& browser_context_id,
    PrefService* pref_service) {
  discard_opt_out_trackers_.emplace(
      std::piecewise_construct, std::forward_as_tuple(browser_context_id),
      std::forward_as_tuple(browser_context_id, pref_service, delegate_.get()));
}

void ProfileDiscardOptOutListHelper::OnProfileWillBeRemovedImpl(
    const std::string& browser_context_id) {
  auto it = discard_opt_out_trackers_.find(browser_context_id);
  CHECK(it != discard_opt_out_trackers_.end(), base::NotFatalUntil::M130);
  discard_opt_out_trackers_.erase(it);
}

}  // namespace performance_manager::user_tuning
