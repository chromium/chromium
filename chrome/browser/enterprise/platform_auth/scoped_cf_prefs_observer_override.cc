// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/scoped_cf_prefs_observer_override.h"

#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/sys_string_conversions.h"

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

FakeCFPreferencesObserver::FakeCFPreferencesObserver(
    base::RepeatingClosure& on_update,
    base::RepeatingCallback<CFPreferencesObserver::RawConfig()> read_config)
    : on_update_(on_update), read_config_(std::move(read_config)) {}

FakeCFPreferencesObserver::~FakeCFPreferencesObserver() = default;

void FakeCFPreferencesObserver::Subscribe(base::RepeatingClosure on_update) {
  *on_update_ = std::move(on_update);
}

void FakeCFPreferencesObserver::Unsubscribe() {
  on_update_->Reset();
}

base::OnceCallback<CFPreferencesObserver::RawConfig()>
FakeCFPreferencesObserver::GetReadConfigCallback() {
  return read_config_;
}

base::apple::ScopedCFTypeRef<CFPropertyListRef> StringsToPropList(
    const std::vector<std::string>& values) {
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> array(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  for (const std::string& value : values) {
    CFArrayAppendValue(array.get(), base::SysUTF8ToCFStringRef(value).get());
  }
  return array;
}

}  // namespace enterprise_auth
