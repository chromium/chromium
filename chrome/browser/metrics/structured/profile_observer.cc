// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/profile_observer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"

namespace metrics::structured {

ProfileObserver::ProfileObserver() {
  profile_manager_observer_.Observe(g_browser_process->profile_manager());
}

ProfileObserver::~ProfileObserver() = default;

void ProfileObserver::OnProfileAdded(Profile* profile) {
  // Non-Regular Profiles consent information are not expected to be
  // observed or checked.
  if (!profiles::IsRegularUserProfile(profile)) {
    return;
  }

  ProfileAdded(*profile);
}

void ProfileObserver::OnProfileManagerDestroying() {
  profile_manager_observer_.Reset();
}

}  // namespace metrics::structured
