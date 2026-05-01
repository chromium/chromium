// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/profile_force_foreground_priority_list_helper.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/performance_manager/execution_context_priority/force_foreground_voter_for_urls.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"

namespace performance_manager::user_tuning {

namespace {

class ProfileForceForegroundPriorityListHelperDelegateImpl
    : public ProfileForceForegroundPriorityListHelper::Delegate {
 public:
  ~ProfileForceForegroundPriorityListHelperDelegateImpl() override = default;

  void SetPatterns(const std::string& browser_context_id,
                   const base::ListValue& patterns) override {
    auto* voter_for_urls =
        execution_context_priority::ForceForegroundVoterForUrls::GetFromGraph(
            PerformanceManager::GetGraph());
    CHECK(voter_for_urls);
    voter_for_urls->SetPatternsForProfile(browser_context_id, patterns);
  }

  void ClearPatterns(const std::string& browser_context_id) override {
    auto* voter_for_urls =
        execution_context_priority::ForceForegroundVoterForUrls::GetFromGraph(
            PerformanceManager::GetGraph());
    CHECK(voter_for_urls);
    voter_for_urls->ClearPatternsForProfile(browser_context_id);
  }
};

}  // namespace

ProfileForceForegroundPriorityListHelper::
    ProfileForceForegroundPriorityTracker::
        ProfileForceForegroundPriorityTracker(
            const std::string& browser_context_id,
            PrefService* pref_service,
            Delegate* delegate)
    : browser_context_id_(browser_context_id), delegate_(delegate) {
  pref_change_registrar_.Init(pref_service);

  pref_change_registrar_.Add(
      performance_manager::user_tuning::prefs::kForceForegroundPriorityForUrls,
      base::BindRepeating(
          &ProfileForceForegroundPriorityListHelper::
              ProfileForceForegroundPriorityTracker::OnPrefChanged,
          base::Unretained(this)));
  // Trigger a first update so the list reflects the initial pref value.
  OnPrefChanged();
}

ProfileForceForegroundPriorityListHelper::
    ProfileForceForegroundPriorityTracker::
        ~ProfileForceForegroundPriorityTracker() {
  delegate_->ClearPatterns(browser_context_id_);
}

void ProfileForceForegroundPriorityListHelper::
    ProfileForceForegroundPriorityTracker::OnPrefChanged() {
  const base::ListValue& urls_value = pref_change_registrar_.prefs()->GetList(
      performance_manager::user_tuning::prefs::kForceForegroundPriorityForUrls);

  if (last_urls_value_ == urls_value) {
    return;
  }
  last_urls_value_ = urls_value.Clone();

  delegate_->SetPatterns(browser_context_id_, urls_value);
}

ProfileForceForegroundPriorityListHelper::
    ProfileForceForegroundPriorityListHelper(std::unique_ptr<Delegate> delegate)
    : delegate_(
          delegate
              ? std::move(delegate)
              : std::make_unique<
                    ProfileForceForegroundPriorityListHelperDelegateImpl>()) {}

ProfileForceForegroundPriorityListHelper::
    ~ProfileForceForegroundPriorityListHelper() = default;

void ProfileForceForegroundPriorityListHelper::OnProfileAdded(
    Profile* profile) {
  OnProfileAddedImpl(profile->UniqueId(), profile->GetPrefs());
}

void ProfileForceForegroundPriorityListHelper::OnProfileWillBeRemoved(
    Profile* profile) {
  OnProfileWillBeRemovedImpl(profile->UniqueId());
}

void ProfileForceForegroundPriorityListHelper::OnProfileAddedImpl(
    const std::string& browser_context_id,
    PrefService* pref_service) {
  trackers_.try_emplace(browser_context_id,
                        std::make_unique<ProfileForceForegroundPriorityTracker>(
                            browser_context_id, pref_service, delegate_.get()));
}

void ProfileForceForegroundPriorityListHelper::OnProfileWillBeRemovedImpl(
    const std::string& browser_context_id) {
  trackers_.erase(browser_context_id);
}

}  // namespace performance_manager::user_tuning
