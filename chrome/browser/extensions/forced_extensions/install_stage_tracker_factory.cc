// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/install_stage_tracker_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace extensions {

// static
InstallStageTracker* InstallStageTrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<InstallStageTracker*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
InstallStageTrackerFactory* InstallStageTrackerFactory::GetInstance() {
  static base::NoDestructor<InstallStageTrackerFactory> instance;
  return instance.get();
}

InstallStageTrackerFactory::InstallStageTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "InstallStageTracker",
          BrowserContextDependencyManager::GetInstance()) {}

InstallStageTrackerFactory::~InstallStageTrackerFactory() = default;

KeyedService* InstallStageTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new InstallStageTracker(context);
}

}  // namespace extensions
