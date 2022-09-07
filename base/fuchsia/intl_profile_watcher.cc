// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/intl_profile_watcher.h"

#include <fuchsia/intl/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <string>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/strings/string_piece.h"

using ::fuchsia::intl::Profile;

namespace base {

FuchsiaIntlProfileWatcher::FuchsiaIntlProfileWatcher(
    ProfileChangeCallback on_profile_changed)
    : FuchsiaIntlProfileWatcher(
          ComponentContextForProcess()
              ->svc()
              ->Connect<::fuchsia::intl::PropertyProvider>(),
          on_profile_changed) {}

FuchsiaIntlProfileWatcher::FuchsiaIntlProfileWatcher(
    ::fuchsia::intl::PropertyProviderPtr property_provider,
    ProfileChangeCallback on_profile_changed)
    : property_provider_(std::move(property_provider)),
      on_profile_changed_(std::move(on_profile_changed)) {
  DCHECK(property_provider_.is_bound());
  DCHECK(on_profile_changed_);

  property_provider_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "intl.PropertyProvider disconnected. "
                          << "Profile changes will not be monitored.";
  });

  property_provider_.events().OnChange = [this]() {
    property_provider_->GetProfile(
        [this](Profile profile) { on_profile_changed_.Run(profile); });
  };
}

FuchsiaIntlProfileWatcher::~FuchsiaIntlProfileWatcher() = default;

// static
std::string FuchsiaIntlProfileWatcher::GetPrimaryTimeZoneIdFromProfile(
    const Profile& profile) {
  if (!profile.has_time_zones()) {
    DLOG(WARNING) << "Profile does not contain time zones.";
    return std::string();
  }

  const std::vector<::fuchsia::intl::TimeZoneId>& time_zones =
      profile.time_zones();
  if (time_zones.empty()) {
    DLOG(ERROR) << "Profile contains an empty time zones list.";
    return std::string();
  }

  return time_zones[0].id;
}

// static
std::string
FuchsiaIntlProfileWatcher::GetPrimaryTimeZoneIdForIcuInitialization() {
  return GetPrimaryTimeZoneIdFromProfile(GetCurrentProfileSync());
}

// static
std::string FuchsiaIntlProfileWatcher::GetPrimaryLocaleIdFromProfile(
    const ::fuchsia::intl::Profile& profile) {
  if (!profile.has_locales()) {
    DLOG(ERROR) << "Profile does not contain locale information.";
    return std::string();
  }

  const std::vector<::fuchsia::intl::LocaleId>& locale_preferences =
      profile.locales();
  if (locale_preferences.empty()) {
    DLOG(ERROR) << "Profile contains an empty locale list.";
    return std::string();
  }

  return locale_preferences[0].id;
}

// static
std::string FuchsiaIntlProfileWatcher::GetPrimaryLocaleIdForInitialization() {
  return GetPrimaryLocaleIdFromProfile(GetCurrentProfileSync());
}

// static
Profile FuchsiaIntlProfileWatcher::GetProfileFromPropertyProvider(
    ::fuchsia::intl::PropertyProviderSyncPtr property_provider) {
  DCHECK(property_provider.is_bound());
  Profile profile;
  zx_status_t status = property_provider->GetProfile(&profile);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "Failed to get intl Profile";
  }
  return profile;
}

// static
::fuchsia::intl::Profile FuchsiaIntlProfileWatcher::GetCurrentProfileSync() {
  ::fuchsia::intl::PropertyProviderSyncPtr provider;
  ComponentContextForProcess()->svc()->Connect(provider.NewRequest());
  return GetProfileFromPropertyProvider(std::move(provider));
}

}  // namespace base
