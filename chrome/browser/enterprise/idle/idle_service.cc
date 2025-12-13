// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/idle_service.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/enterprise/idle/idle_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/enterprise/idle/idle_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/idle_polling_service.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/idle_bubble.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace enterprise_idle {

#if !BUILDFLAG(IS_ANDROID)
// Observes OnBrowserActivated(). If the kIdleTimeoutShowBubbleOnStartup
// pref is true, it shows a bubble when a browser comes into focus. See
// ShowBubbleAction.
class IdleService::BrowserObserver : public BrowserCollectionObserver {
 public:
  explicit BrowserObserver(Profile* profile) : profile_(profile) {
    CHECK_EQ(profile_->GetOriginalProfile(), profile_);
  }

  void StartObserving() {
    if (!observation_.IsObserving()) {
      if (profile_->AllowsBrowserWindows()) {
        observation_.Observe(ProfileBrowserCollection::GetForProfile(profile_));
      }
      if (BrowserWindowInterface* const bwi =
              GetLastActiveBrowserWindowInterfaceWithAnyProfile()) {
        OnBrowserActivatedInternal(bwi);
      }
    }
  }

  void StopObserving() { observation_.Reset(); }

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override {
    OnBrowserActivatedInternal(browser);
  }

 private:
  void OnBrowserActivatedInternal(BrowserWindowInterface* bwi) {
    CHECK(bwi);
    Profile* const profile = bwi->GetProfile();
    auto* prefs = profile->GetPrefs();
    if (bwi->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
        profile == profile_ &&
        prefs->GetBoolean(prefs::kIdleTimeoutShowBubbleOnStartup)) {
      const base::TimeDelta timeout =
          IdleServiceFactory::GetForBrowserContext(profile)->GetTimeout();
      ShowIdleBubble(bwi, timeout, GetActionSet(prefs),
                     base::BindOnce(&IdleService::BrowserObserver::OnClose,
                                    bwi->GetWeakPtr()));
    }
  }

  static void OnClose(base::WeakPtr<BrowserWindowInterface> bwi) {
    if (!bwi) {
      return;
    }
    bwi->GetProfile()->GetPrefs()->SetBoolean(
        prefs::kIdleTimeoutShowBubbleOnStartup, false);
  }

  IdleDialog::ActionSet GetActionSet(PrefService* prefs) {
    std::vector<ActionType> actions;
    std::ranges::transform(prefs->GetList(prefs::kIdleTimeoutActions),
                           std::back_inserter(actions),
                           [](const base::Value& action) {
                             return static_cast<ActionType>(action.GetInt());
                           });
    return ActionsToActionSet(base::flat_set<ActionType>(std::move(actions)));
  }

  const raw_ptr<Profile> profile_;
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      observation_{this};
};
#else
// BrowserObserver for Android, to minimize #ifdef hell.
class IdleService::BrowserObserver {
 public:
  explicit BrowserObserver(Profile* profile) {}
  void StartObserving() {}
  void StopObserving() {}
};
#endif  // !BUILDFLAG(IS_ANDROID)

IdleService::IdleService(Profile* profile)
    : profile_(profile),
      action_runner_(
          std::make_unique<ActionRunner>(profile_,
                                         ActionFactory::GetInstance())) {
  browser_observer_ = std::make_unique<BrowserObserver>(profile);
  DCHECK_EQ(profile_->GetOriginalProfile(), profile_);
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kIdleTimeout,
      base::BindRepeating(&IdleService::OnIdleTimeoutPrefChanged,
                          base::Unretained(this)));
  OnIdleTimeoutPrefChanged();
}

IdleService::~IdleService() = default;

void IdleService::OnIdleTimeoutPrefChanged() {
  base::TimeDelta timeout = GetTimeout();
  if (timeout.is_positive()) {
    // `is_idle_` will auto-update in 1 second, no need to set it here.
    idle_threshold_ = timeout;
    if (!polling_service_observation_.IsObserving()) {
      polling_service_observation_.Observe(
          ui::IdlePollingService::GetInstance());
    }
    browser_observer_->StartObserving();
  } else {
    is_idle_ = false;
    idle_threshold_ = base::TimeDelta();
    polling_service_observation_.Reset();
    browser_observer_->StopObserving();
  }
}

base::TimeDelta IdleService::GetTimeout() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSimulateIdleTimeout)
             ? base::Seconds(5)
             : profile_->GetPrefs()->GetTimeDelta(prefs::kIdleTimeout);
}

void IdleService::OnIdleStateChange(
    const ui::IdlePollingService::State& polled_state) {
  if (is_idle_) {
    if (polled_state.idle_time < idle_threshold_) {
      // Profile just stopped being idle.
      is_idle_ = false;
    }
  } else {
    if (polled_state.idle_time >= idle_threshold_) {
      // Profile just became idle. Run actions.
      is_idle_ = true;
      action_runner_->Run();
    }
  }
}

}  // namespace enterprise_idle
