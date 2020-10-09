// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_INTL_PROFILE_WATCHER_H_
#define BASE_FUCHSIA_INTL_PROFILE_WATCHER_H_

#include <fuchsia/intl/cpp/fidl.h>
#include <string>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/strings/string_piece_forward.h"

namespace base {

// Watches fuchsia.intl.PropertyProvider for change notifications and notifies
// the provided callback. If necessary, the caller is responsible for
// determining whether an actual change of interest has occurred.
class BASE_EXPORT FuchsiaIntlProfileWatcher {
 public:
  using ProfileChangeCallback =
      base::RepeatingCallback<void(const ::fuchsia::intl::Profile&)>;

  // |on_profile_changed| will be called each time the profile may have changed.
  explicit FuchsiaIntlProfileWatcher(ProfileChangeCallback on_profile_changed);

  FuchsiaIntlProfileWatcher(const FuchsiaIntlProfileWatcher&) = delete;
  FuchsiaIntlProfileWatcher& operator=(const FuchsiaIntlProfileWatcher&) =
      delete;
  ~FuchsiaIntlProfileWatcher();

  // Returns the ID of the primary time zone in |profile|.
  // Returns the empty string if the ID cannot be obtained.
  static std::string GetPrimaryTimeZoneIdFromProfile(
      const ::fuchsia::intl::Profile& profile);

  // Returns the ID of the primary time zone for the system.
  // Returns the empty string if the ID cannot be obtained.
  // This is a synchronous blocking call to the system service and should only
  // be used for ICU initialization.
  static std::string GetPrimaryTimeZoneIdForIcuInitialization();

 private:
  friend class GetPrimaryTimeZoneIdFromPropertyProviderTest;
  friend class IntlProfileWatcherTest;

  FuchsiaIntlProfileWatcher(
      ::fuchsia::intl::PropertyProviderPtr property_provider,
      ProfileChangeCallback on_profile_changed);

  // Returns the ID of the primary time zone from the profile obtained from
  // |property_provider|. Returns the empty string if the ID cannot be obtained.
  static std::string GetPrimaryTimeZoneIdFromPropertyProvider(
      ::fuchsia::intl::PropertyProviderSyncPtr property_provider);

  ::fuchsia::intl::PropertyProviderPtr property_provider_;
  const ProfileChangeCallback on_profile_changed_;
};

// TODO(crbug.com/1073821): Remove this block when out-of-tree callers have been
// changed to use the non-fuchsia-sub-namespace version.
namespace fuchsia {

using IntlProfileWatcher = ::base::FuchsiaIntlProfileWatcher;

}  // namespace fuchsia

}  // namespace base

#endif  // BASE_FUCHSIA_INTL_PROFILE_WATCHER_H_
