// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/scoped_cf_prefs_observer_override.h"

#include "base/check_is_test.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"

namespace enterprise_auth {

bool ScopedCFPreferenceObserverOverride::instance_exists_ = false;

ScopedCFPreferenceObserverOverride::ScopedCFPreferenceObserverOverride(
    base::RepeatingCallback<std::unique_ptr<CFPreferencesObserver>()>
        cf_prefs_observer_override) {
  CHECK_IS_TEST();
  CHECK(!instance_exists_);
  instance_exists_ = true;
  ExtensibleEnterpriseSSOPrefsHandler::
      OverrideCFPreferenceObserverForTesting(  // IN-TEST
          std::move(cf_prefs_observer_override));
}

ScopedCFPreferenceObserverOverride::~ScopedCFPreferenceObserverOverride() {
  instance_exists_ = false;
  ExtensibleEnterpriseSSOPrefsHandler::
      OverrideCFPreferenceObserverForTesting(  // IN-TEST
          base::NullCallback());
}

}  // namespace enterprise_auth
