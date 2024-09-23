// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/updater_state.h"

#include <windows.h>

#include <string>
#include <utility>

#include "base/enterprise_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"

namespace component_updater {
namespace {

// Google Update group policy settings.
const wchar_t kGoogleUpdatePoliciesKey[] =
    L"SOFTWARE\\Policies\\Google\\Update";
const wchar_t kCheckPeriodOverrideMinutes[] = L"AutoUpdateCheckPeriodMinutes";
const wchar_t kUpdatePolicyValue[] = L"UpdateDefault";
const wchar_t kChromeUpdatePolicyOverride[] =
    L"Update{8A69D345-D564-463C-AFF1-A69D9E530F96}";

// Don't allow update periods longer than six weeks (Chrome release cadence).
const int kCheckPeriodOverrideMinutesMax = 60 * 24 * 7 * 6;

// Google Update registry settings.
const wchar_t kRegPathGoogleUpdate[] = L"Software\\Google\\Update";
const wchar_t kRegPathClientsGoogleUpdate[] =
    L"Software\\Google\\Update\\Clients\\"
    L"{430FD4D0-B729-4F61-AA34-91526481799D}";
const wchar_t kRegValueGoogleUpdatePv[] = L"pv";
const wchar_t kRegValueLastStartedAU[] = L"LastStartedAU";
const wchar_t kRegValueLastChecked[] = L"LastChecked";

base::Time GetUpdaterTimeValue(bool is_machine, const wchar_t* value_name) {
  const HKEY root_key = is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::win::RegKey update_key;

  if (update_key.Open(root_key, kRegPathGoogleUpdate,
                      KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    DWORD value(0);
    if (update_key.ReadValueDW(value_name, &value) == ERROR_SUCCESS) {
      return base::Time::FromTimeT(value);
    }
  }

  return base::Time();
}

}  // namespace

std::string UpdaterState::StateReaderOmaha::GetUpdaterName() const {
  return std::string("Omaha");
}

base::Version UpdaterState::StateReaderOmaha::GetUpdaterVersion(
    bool is_machine) const {
  const HKEY root_key = is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring version;
  base::win::RegKey key;

  if (key.Open(root_key, kRegPathClientsGoogleUpdate,
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
      key.ReadValue(kRegValueGoogleUpdatePv, &version) == ERROR_SUCCESS) {
    return base::Version(base::WideToUTF8(version));
  }

  return base::Version();
}

bool UpdaterState::StateReaderOmaha::IsAutoupdateCheckEnabled() const {
  return UpdaterState::IsAutoupdateCheckEnabled();
}

base::Time UpdaterState::StateReaderOmaha::GetUpdaterLastStartedAU(
    bool is_machine) const {
  return GetUpdaterTimeValue(is_machine, kRegValueLastStartedAU);
}

base::Time UpdaterState::StateReaderOmaha::GetUpdaterLastChecked(
    bool is_machine) const {
  return GetUpdaterTimeValue(is_machine, kRegValueLastChecked);
}

int UpdaterState::StateReaderOmaha::GetUpdatePolicy() const {
  return UpdaterState::GetUpdatePolicy();
}

update_client::CategorizedError
UpdaterState::StateReaderOmaha::GetLastUpdateCheckError() const {
  return {};
}

bool UpdaterState::IsAutoupdateCheckEnabled() {
  // Check the auto-update check period override. If it is 0 or exceeds the
  // maximum timeout, then for all intents and purposes auto updates are
  // disabled.
  base::win::RegKey policy_key;
  DWORD value = 0;
  if (policy_key.Open(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                      KEY_QUERY_VALUE) == ERROR_SUCCESS &&
      policy_key.ReadValueDW(kCheckPeriodOverrideMinutes, &value) ==
          ERROR_SUCCESS &&
      (value == 0 || value > kCheckPeriodOverrideMinutesMax)) {
    return false;
  }

  return true;
}

// Returns -1 if the policy is not found or the value was invalid. Otherwise,
// returns a value in the [0, 3] range, representing the value of the
// Chrome update group policy.
int UpdaterState::GetUpdatePolicy() {
  const int kMaxUpdatePolicyValue = 3;

  base::win::RegKey policy_key;

  if (policy_key.Open(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                      KEY_QUERY_VALUE) != ERROR_SUCCESS) {
    return -1;
  }

  DWORD value = 0;
  // First try to read the Chrome-specific override.
  if (policy_key.ReadValueDW(kChromeUpdatePolicyOverride, &value) ==
          ERROR_SUCCESS &&
      value <= kMaxUpdatePolicyValue) {
    return value;
  }

  // Try to read default override.
  if (policy_key.ReadValueDW(kUpdatePolicyValue, &value) == ERROR_SUCCESS &&
      value <= kMaxUpdatePolicyValue) {
    return value;
  }

  return -1;
}

}  // namespace component_updater
