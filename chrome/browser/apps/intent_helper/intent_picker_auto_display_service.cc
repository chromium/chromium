// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"

#include <memory>

#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"

class Profile;

namespace {

// Creates and returns the local preference, incrementing, accessing and overall
// checking on the pref's counters are done thru this function and then synced.
std::unique_ptr<IntentPickerAutoDisplayPref> CreateLocalPreference(
    Profile* profile,
    const GURL& url) {
  return std::make_unique<IntentPickerAutoDisplayPref>(
      url, HostContentSettingsMapFactory::GetForProfile(profile));
}

}  // namespace

// static
IntentPickerAutoDisplayService* IntentPickerAutoDisplayService::Get(
    Profile* profile) {
  return IntentPickerAutoDisplayServiceFactory::GetForProfile(profile);
}

IntentPickerAutoDisplayService::IntentPickerAutoDisplayService(Profile* profile)
    : profile_(profile) {}

bool IntentPickerAutoDisplayService::ShouldAutoDisplayUi(const GURL& url) {
  return CreateLocalPreference(profile_, url)->HasExceededThreshold();
}

void IntentPickerAutoDisplayService::IncrementCounter(const GURL& url) {
  CreateLocalPreference(profile_, url)->IncrementCounter();
}

IntentPickerAutoDisplayPref::Platform
IntentPickerAutoDisplayService::GetLastUsedPlatformForTablets(const GURL& url) {
  return CreateLocalPreference(profile_, url)->GetPlatform();
}

void IntentPickerAutoDisplayService::UpdatePlatformForTablets(
    const GURL& url,
    IntentPickerAutoDisplayPref::Platform platform) {
  CreateLocalPreference(profile_, url)->UpdatePlatform(platform);
}
