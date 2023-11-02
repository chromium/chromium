// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/idle_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/idle/idle_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/idle_polling_service.h"

namespace {

bool ProfileHasBrowsers(const Profile* profile) {
  DCHECK(profile);
  profile = profile->GetOriginalProfile();
  return base::ranges::any_of(
      *BrowserList::GetInstance(), [profile](Browser* browser) {
        return browser->profile()->GetOriginalProfile() == profile;
      });
}

// Keeps track of the idle state of each Profile. Keeping all the states in a
// single place means we can do batch actions without duplicating. For instance,
// if 2 Profiles have the same threshold we can close all their windows, wait
// for BOTH profiles to close, and only show the Profile Picker once.
class IdleRegistry : public ui::IdlePollingService::Observer {
 public:
  static IdleRegistry& GetInstance() {
    static base::NoDestructor<IdleRegistry> instance;
    return *instance;
  }

  // Start tracking a new Profile, or update its threshold if it's already being
  // tracked. Called when the IdleProfileCloseTimeout policy changes value.
  void AddOrUpdate(Profile* profile, base::TimeDelta threshold) {
    DCHECK(!profile->IsSystemProfile());
    DCHECK(!profile->IsOffTheRecord());
    if (profiles_.find(profile) == profiles_.end()) {
      profiles_[profile].threshold = threshold;
    }
    if (!polling_service_observation_.IsObserving()) {
      polling_service_observation_.Observe(
          ui::IdlePollingService::GetInstance());
    }
  }

  // Stop tracking a Profile, if it's being tracked. Called during shutdown, or
  // when the IdleProfileCloseTimeout policy becomes unset.
  //
  // If the profile is not tracked, this is a no-op.
  void Remove(Profile* profile) {
    profiles_.erase(profile);
    if (profiles_.empty())
      polling_service_observation_.Reset();
  }

 private:
  friend struct base::DefaultSingletonTraits<IdleRegistry>;

  // See `profiles_`.
  struct ProfileState {
    base::TimeDelta threshold;  // From the IdleProfileCloseTimeout policy.
  };

  // ui::IdlePollingService::Observer:
  void OnIdleStateChange(
      const ui::IdlePollingService::State& polled_state) override {
    for (auto& [profile, state] : profiles_) {
      if (polled_state.idle_time < state.threshold)
        continue;  // Profile is not idle.
      if (base::Contains(closing_profiles_, profile->GetPath()))
        continue;  // Profile is already closing.
      if (!ProfileHasBrowsers(profile))
        continue;  // Can't close a profile with no browsers...

      // Profile just became idle. Trigger idle logic.
      closing_profiles_.insert(profile->GetPath());
      should_open_profile_picker_ = true;

      // Close all browsers for this profile. If there are onbeforeunload
      // handlers, the operation can be aborted (by the user or by JavaScript).
      // This runs OnCloseAborted(), and we don't show the profile picker.
      BrowserList::CloseAllBrowsersWithProfile(
          profile,
          base::BindRepeating(&IdleRegistry::OnCloseSuccess,
                              base::Unretained(this)),
          base::BindRepeating(&IdleRegistry::OnCloseAborted,
                              base::Unretained(this)),
          false);
    }
  }

  void OnCloseAborted(const base::FilePath& profile_dir) {
    // TODO(crbug.com/1316551): What should we do if the profile's been "closed"
    // and *then* a new window is created?
    closing_profiles_.erase(profile_dir);
    // The user (or JavaScript) aborted the close. Don't show the profile
    // picker.
    should_open_profile_picker_ = false;
  }

  // BrowserListObserver:
  void OnCloseSuccess(const base::FilePath& profile_dir) {
    // TODO(crbug.com/1316551): Reset `closing_profiles_` and
    // `should_open_profile_picker_` if something weird happens (e.g. new
    // browser window is created by an extension).
    //
    // TODO(crbug.com/1316551): Technically, a policy refresh could cause the
    // IdleProfileCloseTimeout policy to change its value while browsers are
    // closing. We should try to do something sensible in those cases.
    if (profiles_.empty())
      return;
    if (!base::Contains(closing_profiles_, profile_dir))
      return;

    closing_profiles_.erase(profile_dir);

    if (closing_profiles_.empty() && should_open_profile_picker_) {
      // All windows are done closing for idle profiles. Show the Profile
      // Picker.
      should_open_profile_picker_ = false;
      ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
          ProfilePicker::EntryPoint::kProfileIdle));
    }
  }

  // Set of profiles being closed right now. Filled in when idle logic triggers,
  // and becomes empty again when:
  //
  // (a) All idle browsers finish closing.
  // (b) The user aborts closing by clicking "don't leave" on an in-progress
  //     form.
  std::set<base::FilePath> closing_profiles_;
  // Whether to open the profile picker after the last profile in
  // `closing_profiles_` finishes closing.
  bool should_open_profile_picker_ = false;

  std::map<Profile*, ProfileState> profiles_;

  base::ScopedObservation<ui::IdlePollingService,
                          ui::IdlePollingService::Observer>
      polling_service_observation_{this};
};

}  // namespace

namespace enterprise_idle {

IdleService::IdleService(Profile* profile) : profile_(profile) {
  DCHECK_EQ(profile_->GetOriginalProfile(), profile_);
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kIdleProfileCloseTimeout,
      base::BindRepeating(&IdleService::OnIdleProfileCloseTimeoutPrefChanged,
                          base::Unretained(this)));
  OnIdleProfileCloseTimeoutPrefChanged();
}

IdleService::~IdleService() {
  IdleRegistry::GetInstance().Remove(profile_);
}

void IdleService::Shutdown() {
  IdleRegistry::GetInstance().Remove(profile_);
}

void IdleService::OnIdleProfileCloseTimeoutPrefChanged() {
  int minutes =
      profile_->GetPrefs()->GetInteger(prefs::kIdleProfileCloseTimeout);
  if (minutes > 0) {
    IdleRegistry::GetInstance().AddOrUpdate(
        profile_, std::max(base::Minutes(5), base::Minutes(minutes)));
  } else {
    IdleRegistry::GetInstance().Remove(profile_);
  }
}

}  // namespace enterprise_idle
