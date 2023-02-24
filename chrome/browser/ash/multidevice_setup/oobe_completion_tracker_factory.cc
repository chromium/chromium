// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/multidevice_setup/oobe_completion_tracker_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {
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
    : ProfileKeyedServiceFactory(
          "OobeCompletionTrackerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

OobeCompletionTrackerFactory::~OobeCompletionTrackerFactory() = default;

KeyedService* OobeCompletionTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OobeCompletionTracker();
}

}  // namespace multidevice_setup
}  // namespace ash
