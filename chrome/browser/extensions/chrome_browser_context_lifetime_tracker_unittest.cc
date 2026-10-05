// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_browser_context_lifetime_tracker.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/browser_context_lifetime_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

enum class Event {
  kOffTheRecordCreated,
  kOffTheRecordDestroyed,
  kPrimaryDestroyed,
};

using EventLog = std::vector<std::pair<Event, content::BrowserContext*>>;

// Records every notification it receives, and stops observing on
// destruction as required by BrowserContextLifetimeTracker.
class RecordingObserver : public BrowserContextLifetimeObserver {
 public:
  RecordingObserver() = default;
  RecordingObserver(const RecordingObserver&) = delete;
  RecordingObserver& operator=(const RecordingObserver&) = delete;
  ~RecordingObserver() override { tracker().StopObserving(*this); }

  void StartObserving(Profile& profile) {
    tracker().StartObserving(profile, *this);
  }
  void StopObserving() { tracker().StopObserving(*this); }

  const EventLog& events() const { return events_; }

 private:
  static ChromeBrowserContextLifetimeTracker& tracker() {
    return *ChromeBrowserContextLifetimeTracker::GetInstance();
  }

  // BrowserContextLifetimeObserver:
  void OnRelatedOffTheRecordBrowserContextCreated(
      content::BrowserContext& off_the_record_context) override {
    events_.emplace_back(Event::kOffTheRecordCreated, &off_the_record_context);
  }
  void OnRelatedOffTheRecordBrowserContextDestroyed(
      content::BrowserContext& off_the_record_context) override {
    events_.emplace_back(Event::kOffTheRecordDestroyed,
                         &off_the_record_context);
  }
  void OnPrimaryBrowserContextDestroyed(
      content::BrowserContext& primary_context) override {
    events_.emplace_back(Event::kPrimaryDestroyed, &primary_context);
  }

  EventLog events_;
};

// Starts observing `profile` with `observer` from inside
// OnOffTheRecordProfileCreated(), i.e. while the profile is still
// dispatching the creation of its primary OTR profile.
class StartObservingOnOffTheRecordCreated : public ProfileObserver {
 public:
  StartObservingOnOffTheRecordCreated(Profile& profile,
                                      RecordingObserver& observer)
      : observer_(observer) {
    observation_.Observe(&profile);
  }

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override {
    observation_.Reset();
    observer_->StartObserving(*off_the_record->GetOriginalProfile());
  }

 private:
  const raw_ref<RecordingObserver> observer_;
  base::ScopedObservation<Profile, ProfileObserver> observation_{this};
};

}  // namespace

class ChromeBrowserContextLifetimeTrackerTest : public testing::Test {
 protected:
  void SetUp() override { profile_ = TestingProfile::Builder().Build(); }

  TestingProfile* profile() { return profile_.get(); }
  Profile* CreatePrimaryOTRProfile() {
    return profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

// An OTR profile that already exists is reported synchronously from
// StartObserving().
TEST_F(ChromeBrowserContextLifetimeTrackerTest, ExistingOffTheRecordProfile) {
  Profile* otr_profile = CreatePrimaryOTRProfile();

  RecordingObserver observer;
  observer.StartObserving(*profile());

  EXPECT_THAT(observer.events(),
              ElementsAre(Pair(Event::kOffTheRecordCreated, otr_profile)));
}

// The primary OTR profile is reported every time it is created and
// destroyed while the original profile is alive.
TEST_F(ChromeBrowserContextLifetimeTrackerTest,
       OffTheRecordProfileCreatedAndDestroyed) {
  RecordingObserver observer;
  observer.StartObserving(*profile());
  EXPECT_THAT(observer.events(), IsEmpty());

  Profile* otr_profile = CreatePrimaryOTRProfile();
  profile()->DestroyOffTheRecordProfile(otr_profile);
  Profile* new_otr_profile = CreatePrimaryOTRProfile();

  EXPECT_THAT(observer.events(),
              ElementsAre(Pair(Event::kOffTheRecordCreated, otr_profile),
                          Pair(Event::kOffTheRecordDestroyed, otr_profile),
                          Pair(Event::kOffTheRecordCreated, new_otr_profile)));
}

// Non-primary OTR profiles are not reported.
TEST_F(ChromeBrowserContextLifetimeTrackerTest,
       NonPrimaryOffTheRecordProfileIgnored) {
  Profile* existing_otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  RecordingObserver observer;
  observer.StartObserving(*profile());

  Profile* new_otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  profile()->DestroyOffTheRecordProfile(existing_otr_profile);
  profile()->DestroyOffTheRecordProfile(new_otr_profile);

  EXPECT_THAT(observer.events(), IsEmpty());
}

// No notifications are sent after StopObserving().
TEST_F(ChromeBrowserContextLifetimeTrackerTest, StopObserving) {
  RecordingObserver observer;
  observer.StartObserving(*profile());
  observer.StopObserving();

  profile()->DestroyOffTheRecordProfile(CreatePrimaryOTRProfile());
  profile_.reset();

  EXPECT_THAT(observer.events(), IsEmpty());
}

// The original profile sends OnProfileWillBeDestroyed() before destroying
// its OTR profiles (see ProfileImpl::~ProfileImpl() and ProfileDestroyer).
// The OTR profile's destruction must still be reported first, and nothing
// may follow OnPrimaryBrowserContextDestroyed().
TEST_F(ChromeBrowserContextLifetimeTrackerTest,
       PrimaryDestroyedWithOffTheRecordProfileAlive) {
  Profile* otr_profile = CreatePrimaryOTRProfile();
  RecordingObserver observer;
  observer.StartObserving(*profile());

  // TestingProfile destroys its OTR profiles before notifying, unlike
  // ProfileImpl, so send the notification first to match production.
  Profile* original_profile = profile();
  profile()->MaybeSendDestroyedNotification();
  profile_.reset();

  EXPECT_THAT(observer.events(),
              ElementsAre(Pair(Event::kOffTheRecordCreated, otr_profile),
                          Pair(Event::kOffTheRecordDestroyed, otr_profile),
                          Pair(Event::kPrimaryDestroyed, original_profile)));
}

// StartObserving() may be called while the original profile is dispatching
// OnOffTheRecordProfileCreated(). The new primary OTR profile is already
// registered at that point, and the new watcher is notified as well, so it
// must not observe the OTR profile twice.
TEST_F(ChromeBrowserContextLifetimeTrackerTest,
       StartObservingDuringOffTheRecordProfileCreation) {
  RecordingObserver observer;
  StartObservingOnOffTheRecordCreated starter(*profile(), observer);

  Profile* otr_profile = CreatePrimaryOTRProfile();

  EXPECT_THAT(observer.events(),
              ElementsAre(Pair(Event::kOffTheRecordCreated, otr_profile)));

  profile()->DestroyOffTheRecordProfile(otr_profile);
  EXPECT_THAT(observer.events(),
              ElementsAre(Pair(Event::kOffTheRecordCreated, otr_profile),
                          Pair(Event::kOffTheRecordDestroyed, otr_profile)));
}

// For a guest profile, the original guest profile is observed and its
// primary OTR profile, where guest browsing happens, is reported.
TEST_F(ChromeBrowserContextLifetimeTrackerTest, GuestProfile) {
  std::unique_ptr<TestingProfile> guest_profile =
      TestingProfile::Builder().SetGuestSession().Build();
  ASSERT_FALSE(guest_profile->IsOffTheRecord());

  RecordingObserver observer;
  observer.StartObserving(*guest_profile);

  Profile* guest_otr_profile =
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_TRUE(guest_otr_profile->IsGuestSession());

  Profile* original_profile = guest_profile.get();
  guest_profile->MaybeSendDestroyedNotification();
  guest_profile.reset();

  EXPECT_THAT(
      observer.events(),
      ElementsAre(Pair(Event::kOffTheRecordCreated, guest_otr_profile),
                  Pair(Event::kOffTheRecordDestroyed, guest_otr_profile),
                  Pair(Event::kPrimaryDestroyed, original_profile)));
}

}  // namespace extensions
