// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

// static
LockedSessionWindowTrackerFactory*
LockedSessionWindowTrackerFactory::GetInstance() {
  return base::Singleton<LockedSessionWindowTrackerFactory>::get();
}

// static
LockedSessionWindowTracker*
LockedSessionWindowTrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LockedSessionWindowTracker*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

LockedSessionWindowTrackerFactory::LockedSessionWindowTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "LockedSessionWindowTracker",
          BrowserContextDependencyManager::GetInstance()) {}

LockedSessionWindowTrackerFactory::~LockedSessionWindowTrackerFactory() =
    default;

std::unique_ptr<KeyedService>
LockedSessionWindowTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  PrefService* const pref_service = user_prefs::UserPrefs::Get(context);
  auto url_blocklist_manager = std::make_unique<policy::URLBlocklistManager>(
      pref_service, policy::policy_prefs::kUrlBlocklist,
      policy::policy_prefs::kUrlAllowlist);
  auto on_task_blocklist =
      std::make_unique<OnTaskBlocklist>(std::move(url_blocklist_manager));
  return std::make_unique<LockedSessionWindowTracker>(
      std::move(on_task_blocklist));
}

content::BrowserContext*
LockedSessionWindowTrackerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
