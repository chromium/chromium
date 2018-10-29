// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/oobe_completion_tracker_factory.h"

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace multidevice_setup {

// static
OobeCompletionTracker* OobeCompletionTrackerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OobeCompletionTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
OobeCompletionTrackerFactory* OobeCompletionTrackerFactory::GetInstance() {
  return base::Singleton<OobeCompletionTrackerFactory>::get();
}

OobeCompletionTrackerFactory::OobeCompletionTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "OobeCompletionTrackerFactory",
          BrowserContextDependencyManager::GetInstance()) {}

OobeCompletionTrackerFactory::~OobeCompletionTrackerFactory() = default;

KeyedService* OobeCompletionTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OobeCompletionTracker();
}

}  // namespace multidevice_setup

}  // namespace chromeos
