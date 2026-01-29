// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"

#include <memory>
#include <optional>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace enterprise_auth {

using ScopedPropList = base::apple::ScopedCFTypeRef<CFPropertyListRef>;

namespace {

const CFStringRef kExtensibleSSOPrefName(CFSTR("com.apple.extensiblesso"));
base::NoDestructor<
    base::RepeatingCallback<std::unique_ptr<CFPreferencesObserver>()>>
    g_cf_prefs_observer_override_for_testing;

base::ListValue ParseConfiguration(CFPreferencesObserver::Config config) {
  if (!config.extension_id || !config.team_id || !config.hosts) {
    return {};
  }

  // This mechanism is meant to be used only for Okta's SSO extension.
  // If the extension and team IDs don't match return an empty result.
  const CFStringRef extension_id =
      base::apple::CFCast<CFStringRef>(config.extension_id.get());
  if (!extension_id ||
      !CFEqual(extension_id,
               ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOExtensionID)) {
    return {};
  }

  const CFStringRef team_id =
      base::apple::CFCast<CFStringRef>(config.team_id.get());
  if (!team_id ||
      !CFEqual(team_id, ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOTeamID)) {
    return {};
  }

  const CFArrayRef array = base::apple::CFCast<CFArrayRef>(config.hosts.get());
  if (!array) {
    return {};
  }
  const CFIndex size = CFArrayGetCount(array);
  base::ListValue hostnames;
  hostnames.reserve(size);

  for (CFIndex i = 0; i < size; ++i) {
    CFStringRef cf_hostname =
        base::apple::CFCast<CFStringRef>(CFArrayGetValueAtIndex(array, i));
    if (!cf_hostname) {
      continue;
    }
    std::string hostname = base::SysCFStringRefToUTF8(cf_hostname);
    if (!hostname.empty()) {
      hostnames.Append(std::move(hostname));
    }
  }

  return hostnames;
}

base::ListValue ReadAndParseConfiguration(
    base::OnceCallback<CFPreferencesObserver::Config()> read_callback) {
  CFPreferencesObserver::Config config = std::move(read_callback).Run();
  return ParseConfiguration(std::move(config));
}

}  // namespace

CFPreferencesObserver::Config::Config(ScopedPropList extension_id,
                                      ScopedPropList team_id,
                                      ScopedPropList hosts)
    : extension_id(std::move(extension_id)),
      team_id(std::move(team_id)),
      hosts(std::move(hosts)) {}
CFPreferencesObserver::Config::Config(const Config&) = default;
CFPreferencesObserver::Config::Config(Config&&) = default;
CFPreferencesObserver::Config& CFPreferencesObserver::Config::operator=(
    const Config&) = default;
CFPreferencesObserver::Config& CFPreferencesObserver::Config::operator=(
    Config&&) = default;
CFPreferencesObserver::Config::~Config() = default;

class CFPreferencesObserverImpl final : public CFPreferencesObserver {
 public:
  ~CFPreferencesObserverImpl() override { Unsubscribe(); }

  static void OnNotification(CFNotificationCenterRef center,
                             void* observer,
                             CFStringRef name,
                             const void* object,
                             CFDictionaryRef userInfo) {
    if (CFEqual(name, kExtensibleSSOPrefName)) {
      CFPreferencesObserverImpl* instance =
          static_cast<CFPreferencesObserverImpl*>(observer);
      instance->callback_.Run();
    }
  }

  void Subscribe(base::RepeatingClosure on_update) override {
    if (!callback_) {
      callback_ = std::move(on_update);
      CFNotificationCenterAddObserver(
          CFNotificationCenterGetDarwinNotifyCenter(), this,
          &CFPreferencesObserverImpl::OnNotification, kExtensibleSSOPrefName,
          nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    }
  }

  void Unsubscribe() override {
    if (callback_) {
      CFNotificationCenterRemoveObserver(
          CFNotificationCenterGetDarwinNotifyCenter(), this,
          kExtensibleSSOPrefName, nullptr);
      callback_.Reset();
    }
  }

  base::OnceCallback<Config()> GetReadConfigCallback() override {
    return base::BindOnce([]() {
      auto extension_id = ScopedPropList(CFPreferencesCopyAppValue(
          CFSTR("ExtensionIdentifier"), kExtensibleSSOPrefName));
      auto hosts = ScopedPropList(
          CFPreferencesCopyAppValue(CFSTR("Hosts"), kExtensibleSSOPrefName));
      auto team_id = ScopedPropList(CFPreferencesCopyAppValue(
          CFSTR("TeamIdentifier"), kExtensibleSSOPrefName));
      return Config(std::move(extension_id), std::move(team_id),
                    std::move(hosts));
    });
  }

 private:
  base::RepeatingClosure callback_;
};

// Team ID and Extension ID are fields of the MDM profile for the Apple
// Extensible Enterprise SSO payload (com.apple.extensiblesso).
// Team ID identifies the IdP (Okta) and the Extension ID identifies the
// specific SSO extension app on the device.
//
// These constants are used to filter the system configuration: we only
// sync hosts that are explicitly configured to use the Okta extension.
//
// The concrete values can be found in Okta's official documentation.
const CFStringRef ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOExtensionID(
    CFSTR("com.okta.mobile.auth-service-extension"));
const CFStringRef ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOTeamID(
    CFSTR("B7F62B65BN"));

ExtensibleEnterpriseSSOPrefsHandler::ExtensibleEnterpriseSSOPrefsHandler(
    PrefService* local_state)
    : local_state_(local_state) {
  if (*g_cf_prefs_observer_override_for_testing) {
    CHECK_IS_TEST();
    cf_preferences_observer_ = g_cf_prefs_observer_override_for_testing->Run();
  } else {
    cf_preferences_observer_ = std::make_unique<CFPreferencesObserverImpl>();
  }

  DCHECK(cf_preferences_observer_);
  DCHECK(local_state_);
  auto callback =
      base::BindRepeating(&ExtensibleEnterpriseSSOPrefsHandler::UpdatePrefs,
                          weak_ptr_factory_.GetWeakPtr());
  auto thread_safe_callback =
      base::BindPostTaskToCurrentDefault(std::move(callback));
  cf_preferences_observer_->Subscribe(std::move(thread_safe_callback));
}

ExtensibleEnterpriseSSOPrefsHandler::~ExtensibleEnterpriseSSOPrefsHandler() {
  DCHECK(cf_preferences_observer_);
  cf_preferences_observer_->Unsubscribe();
}

void ExtensibleEnterpriseSSOPrefsHandler::UpdatePrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(local_state_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadAndParseConfiguration,
                     cf_preferences_observer_->GetReadConfigCallback()),
      base::BindOnce(&ExtensibleEnterpriseSSOPrefsHandler::OnConfigRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExtensibleEnterpriseSSOPrefsHandler::OnConfigRead(base::ListValue res) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_state_->SetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts,
                        std::move(res));
}

// static
void ExtensibleEnterpriseSSOPrefsHandler::RegisterPrefs(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterListPref(
      prefs::kExtensibleEnterpriseSSOConfiguredHosts);
}

// static
void ExtensibleEnterpriseSSOPrefsHandler::
    OverrideCFPreferenceObserverForTesting(
        base::RepeatingCallback<std::unique_ptr<CFPreferencesObserver>()>
            cf_prefs_observer_override) {
  *g_cf_prefs_observer_override_for_testing =
      std::move(cf_prefs_observer_override);
}

}  // namespace enterprise_auth
