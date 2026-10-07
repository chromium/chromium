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
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
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
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_metadata.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"

namespace enterprise_auth {

using ScopedPropList = base::apple::ScopedCFTypeRef<CFPropertyListRef>;

namespace {

const CFStringRef kExtensibleSsoPrefName(CFSTR("com.apple.extensiblesso"));
const CFStringRef kExtensionIdentifierKey(CFSTR("ExtensionIdentifier"));
const CFStringRef kTeamIdentifierKey(CFSTR("TeamIdentifier"));
const CFStringRef kHostsKey(CFSTR("Hosts"));

base::RepeatingCallback<std::unique_ptr<CFPreferencesObserver>()>&
GetCfPrefsOverrideForTesting() {
  static base::NoDestructor<
      base::RepeatingCallback<std::unique_ptr<CFPreferencesObserver>()>>
      cf_prefs_observer_override_for_testing;
  return *cf_prefs_observer_override_for_testing;
}

// Stub implementation for tests that don't explicitly use an override.
class StubCFPreferencesObserver : public CFPreferencesObserver {
 public:
  StubCFPreferencesObserver() {}
  void Subscribe(base::RepeatingClosure on_update) override {}
  void Unsubscribe() override {}
  base::OnceCallback<RawConfig()> GetReadConfigCallback() override {
    return base::BindOnce([]() {
      return RawConfig(ScopedPropList(nullptr), ScopedPropList(nullptr),
                       ScopedPropList(nullptr));
    });
  }
};

class CFPreferencesObserverImpl final : public CFPreferencesObserver {
 public:
  CFPreferencesObserverImpl() { CHECK_IS_NOT_TEST(); }
  ~CFPreferencesObserverImpl() override { Unsubscribe(); }

  static void OnNotification(CFNotificationCenterRef center,
                             void* observer,
                             CFStringRef name,
                             const void* object,
                             CFDictionaryRef userInfo) {
    if (CFEqual(name, kExtensibleSsoPrefName)) {
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
          &CFPreferencesObserverImpl::OnNotification, kExtensibleSsoPrefName,
          nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    }
  }

  void Unsubscribe() override {
    if (callback_) {
      CFNotificationCenterRemoveObserver(
          CFNotificationCenterGetDarwinNotifyCenter(), this,
          kExtensibleSsoPrefName, nullptr);
      callback_.Reset();
    }
  }

  base::OnceCallback<RawConfig()> GetReadConfigCallback() override {
    return base::BindOnce([]() {
      auto extension_id = ScopedPropList(CFPreferencesCopyAppValue(
          kExtensionIdentifierKey, kExtensibleSsoPrefName));
      auto hosts = ScopedPropList(
          CFPreferencesCopyAppValue(kHostsKey, kExtensibleSsoPrefName));
      auto team_id = ScopedPropList(CFPreferencesCopyAppValue(
          kTeamIdentifierKey, kExtensibleSsoPrefName));
      return RawConfig(std::move(extension_id), std::move(team_id),
                       std::move(hosts));
    });
  }

 private:
  base::RepeatingClosure callback_;
};

std::unique_ptr<CFPreferencesObserver> CreateCfPreferencesObserver() {
  if (GetCfPrefsOverrideForTesting()) {
    CHECK_IS_TEST();
    return GetCfPrefsOverrideForTesting().Run();  // IN-TEST
  } else {
    // This class is used to make an OS call on browser process's construction,
    // which would cause the OS call to be made in browser tests that don't
    // directly override the prefs handler.
    const auto* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kBrowserTest) ||
        command_line->HasSwitch(switches::kTestType) ||
        command_line->GetProgram().BaseName().value().find(FILE_PATH_LITERAL(
            "interactive_ui_tests")) != base::FilePath::StringType::npos) {
      // Real implementation should never be used in tests.
      return std::make_unique<StubCFPreferencesObserver>();
    } else {
      return std::make_unique<CFPreferencesObserverImpl>();
    }
  }
}

std::optional<ExtensibleEnterpriseSSOPrefsHandler::Config> ParseConfiguration(
    CFPreferencesObserver::RawConfig config) {
  if (!config.extension_id || !config.team_id || !config.hosts) {
    VLOG_POLICY(1, EXTENSIBLE_SSO)
        << "SSO extension MDM payload not found or incomplete.";
    return std::nullopt;
  }

  // If the extension or team IDs don't match any supported IdPs return an empty
  // result.
  const CFStringRef extension_id =
      base::apple::CFCast<CFStringRef>(config.extension_id.get());
  if (!extension_id || CFStringGetLength(extension_id) == 0) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO) << "Failed to parse SSO extension MDM "
                                           "payload: extension_id is invalid.";
    return std::nullopt;
  }

  const CFStringRef team_id =
      base::apple::CFCast<CFStringRef>(config.team_id.get());
  if (!team_id || CFStringGetLength(team_id) == 0) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO) << "Failed to parse SSO extension MDM "
                                           "payload: team_id is invalid.";
    return std::nullopt;
  }

  // Only Okta is supported for now since request proxying in
  // `PlatformAuthProxyingURLLoaderFactory` only handles Okta SSO requests.
  const SsoExtensionMetadata* metadata =
      FindSsoExtensionMetadata(team_id, extension_id);
  if (!metadata || metadata->idp_name != kOktaIdentityProvider) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO)
        << "Failed to parse SSO extension MDM "
           "payload: extension does not match any supported IdP.";
    return std::nullopt;
  }

  const CFArrayRef array = base::apple::CFCast<CFArrayRef>(config.hosts.get());
  if (!array) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO) << "Failed to parse SSO extension MDM "
                                           "payload: hosts is not an array.";
    return std::nullopt;
  }

  std::string extension_id_string = base::SysCFStringRefToUTF8(extension_id);
  std::string team_id_string = base::SysCFStringRefToUTF8(team_id);
  if (extension_id_string.empty() || team_id_string.empty()) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO)
        << "Failed to parse SSO extension MDM "
           "payload: couldn't convert CFStringRef to UTF-8.";
    return std::nullopt;
  }

  const CFIndex size = CFArrayGetCount(array);
  std::vector<std::string> hostnames;
  hostnames.reserve(size);
  for (CFIndex i = 0; i < size; ++i) {
    CFStringRef cf_hostname =
        base::apple::CFCast<CFStringRef>(CFArrayGetValueAtIndex(array, i));
    if (!cf_hostname) {
      continue;
    }
    std::string hostname = base::SysCFStringRefToUTF8(cf_hostname);
    if (!hostname.empty()) {
      hostnames.push_back(std::move(hostname));
    }
  }

  return ExtensibleEnterpriseSSOPrefsHandler::Config{
      .extension_id = std::move(extension_id_string),
      .team_id = std::move(team_id_string),
      .hosts = std::move(hostnames),
  };
}

std::optional<ExtensibleEnterpriseSSOPrefsHandler::Config>
ReadAndParseConfiguration(
    base::OnceCallback<CFPreferencesObserver::RawConfig()> read_callback) {
  CFPreferencesObserver::RawConfig config = std::move(read_callback).Run();
  return ParseConfiguration(std::move(config));
}

}  // namespace

CFPreferencesObserver::RawConfig::RawConfig(ScopedPropList extension_id,
                                            ScopedPropList team_id,
                                            ScopedPropList hosts)
    : extension_id(std::move(extension_id)),
      team_id(std::move(team_id)),
      hosts(std::move(hosts)) {}
CFPreferencesObserver::RawConfig::RawConfig(const RawConfig&) = default;
CFPreferencesObserver::RawConfig::RawConfig(RawConfig&&) = default;
CFPreferencesObserver::RawConfig& CFPreferencesObserver::RawConfig::operator=(
    const RawConfig&) = default;
CFPreferencesObserver::RawConfig& CFPreferencesObserver::RawConfig::operator=(
    RawConfig&&) = default;
CFPreferencesObserver::RawConfig::~RawConfig() = default;

ExtensibleEnterpriseSSOPrefsHandler::ExtensibleEnterpriseSSOPrefsHandler(
    PrefService* local_state)
    : cf_preferences_observer_(CreateCfPreferencesObserver()),
      local_state_(local_state) {
  CHECK(cf_preferences_observer_, base::NotFatalUntil::M161);
  CHECK(local_state_, base::NotFatalUntil::M161);
  auto callback =
      base::BindRepeating(&ExtensibleEnterpriseSSOPrefsHandler::UpdatePrefs,
                          weak_ptr_factory_.GetWeakPtr());
  auto thread_safe_callback =
      base::BindPostTaskToCurrentDefault(std::move(callback));
  cf_preferences_observer_->Subscribe(std::move(thread_safe_callback));
}

ExtensibleEnterpriseSSOPrefsHandler::~ExtensibleEnterpriseSSOPrefsHandler() {
  CHECK(cf_preferences_observer_, base::NotFatalUntil::M161);
  cf_preferences_observer_->Unsubscribe();
}

void ExtensibleEnterpriseSSOPrefsHandler::UpdatePrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(local_state_, base::NotFatalUntil::M161);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadAndParseConfiguration,
                     cf_preferences_observer_->GetReadConfigCallback()),
      base::BindOnce(&ExtensibleEnterpriseSSOPrefsHandler::OnConfigRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExtensibleEnterpriseSSOPrefsHandler::OnConfigRead(
    std::optional<Config> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!config.has_value()) {
    local_state_->SetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts, {});
    return;
  }

  VLOG_POLICY(1, EXTENSIBLE_SSO)
      << "Successfully parsed SSO extension MDM payload.";

  base::ListValue hosts;
  hosts.reserve(config->hosts.size());
  for (const std::string& host : config->hosts) {
    hosts.Append(host);
  }
  local_state_->SetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts,
                        std::move(hosts));
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
  GetCfPrefsOverrideForTesting() =  // IN-TEST
      std::move(cf_prefs_observer_override);
}

}  // namespace enterprise_auth
