// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_browser_context_lifetime_tracker.h"

#include <utility>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"

namespace extensions {

// Observes a Profile and its primary off-the-record Profile (if any),
// forwarding their creation/destruction to a BrowserContextLifetimeObserver
// in terms of content::BrowserContext.
class ChromeBrowserContextLifetimeTracker::ProfileWatcher
    : public ProfileObserver {
 public:
  ProfileWatcher(Profile& profile, BrowserContextLifetimeObserver& observer)
      : observer_(observer) {
    profile_observation_.Observe(&profile);
    if (profile.HasPrimaryOTRProfile()) {
      ObserveOffTheRecordProfile(
          *profile.GetPrimaryOTRProfile(/*create_if_needed=*/false));
    }
  }

  ProfileWatcher(const ProfileWatcher&) = delete;
  ProfileWatcher& operator=(const ProfileWatcher&) = delete;

  ~ProfileWatcher() override = default;

 private:
  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override {
    // TODO(crbug.com/417228685): Clank allows for multiple OTR profiles,
    // unlike desktop Chrome. Extensions APIs may have built-in assumptions
    // that there will only be one OTR profile. We need to determine how this
    // will be handled in Desktop Android.
    if (!off_the_record->IsPrimaryOTRProfile()) {
      return;
    }
    ObserveOffTheRecordProfile(*off_the_record);
  }

  void OnProfileWillBeDestroyed(Profile* profile) override {
    if (profile_observation_.IsObservingSource(profile)) {
      // The original profile sends this notification before it destroys its
      // OTR profiles, so report the OTR profile's destruction first: the
      // observer must not receive anything after
      // OnPrimaryBrowserContextDestroyed().
      if (otr_profile_observation_.IsObserving()) {
        Profile* otr_profile = otr_profile_observation_.GetSource();
        otr_profile_observation_.Reset();
        observer_->OnRelatedOffTheRecordBrowserContextDestroyed(*otr_profile);
      }
      profile_observation_.Reset();
      observer_->OnPrimaryBrowserContextDestroyed(*profile);
    } else if (otr_profile_observation_.IsObservingSource(profile)) {
      otr_profile_observation_.Reset();
      observer_->OnRelatedOffTheRecordBrowserContextDestroyed(*profile);
    }
  }

  void ObserveOffTheRecordProfile(Profile& otr_profile) {
    // The primary OTR profile is registered with its parent before
    // OnOffTheRecordProfileCreated() is dispatched, and a ProfileObserver
    // added during that dispatch is also notified. So if this watcher was
    // created from another observer's OnOffTheRecordProfileCreated(), it
    // has already picked up `otr_profile` in its constructor and must not
    // observe it (or notify `observer_`) a second time.
    if (otr_profile_observation_.IsObservingSource(&otr_profile)) {
      return;
    }
    otr_profile_observation_.Observe(&otr_profile);
    observer_->OnRelatedOffTheRecordBrowserContextCreated(otr_profile);
  }

  const raw_ref<BrowserContextLifetimeObserver> observer_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  base::ScopedObservation<Profile, ProfileObserver> otr_profile_observation_{
      this};
};

// static
ChromeBrowserContextLifetimeTracker*
ChromeBrowserContextLifetimeTracker::GetInstance() {
  static base::NoDestructor<ChromeBrowserContextLifetimeTracker> instance;
  return instance.get();
}

ChromeBrowserContextLifetimeTracker::ChromeBrowserContextLifetimeTracker() =
    default;

ChromeBrowserContextLifetimeTracker::~ChromeBrowserContextLifetimeTracker() =
    default;

void ChromeBrowserContextLifetimeTracker::StartObserving(
    content::BrowserContext& context,
    BrowserContextLifetimeObserver& observer) {
  // `observer` must not already be watching a profile: every ProfileWatcher
  // this creates starts a fresh ScopedObservation of `profile`'s OTR
  // sibling, so allowing a second one for the same `observer` would either
  // silently replace (and stop observing via) the first one, or, if it
  // reused the same ProfileWatcher, double-add the OTR observation --- the
  // root cause of the DumpWithoutCrashing in crbug.com/472076020. This
  // DCHECK guards against that class of bug being reintroduced, e.g. by a
  // caller invoking StartObserving() again on a reconnect.
  DCHECK(watchers_.find(&observer) == watchers_.end());
  Profile* profile = Profile::FromBrowserContext(&context);
  // `context` must be an original (non-off-the-record) profile. For an OTR
  // profile, HasPrimaryOTRProfile()/GetPrimaryOTRProfile() resolve to the
  // profile itself (or, for a non-primary OTR profile, to its parent's primary
  // OTR profile), and ProfileObserver::OnOffTheRecordProfileCreated() is only
  // dispatched on the original profile, so the watcher would not behave
  // sensibly. Guest profiles are fine: the original guest profile is passed
  // in and its primary OTR profile (where guest browsing happens) is reported
  // as the related off-the-record context.
  CHECK(!profile->IsOffTheRecord());
  watchers_[&observer] = std::make_unique<ProfileWatcher>(*profile, observer);
}

void ChromeBrowserContextLifetimeTracker::StopObserving(
    BrowserContextLifetimeObserver& observer) {
  watchers_.erase(&observer);
}

}  // namespace extensions
