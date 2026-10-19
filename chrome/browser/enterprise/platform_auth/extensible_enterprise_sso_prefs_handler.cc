// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
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
#include "url/gurl.h"

namespace enterprise_auth {

using ScopedPropList = base::apple::ScopedCFTypeRef<CFPropertyListRef>;

namespace {

// The specification of the com.apple.extensiblesso payload can be found at
// https://github.com/apple/device-management/blob/release/mdm/profiles/com.apple.extensiblesso.yaml.
const CFStringRef kExtensibleSsoPrefName(CFSTR("com.apple.extensiblesso"));
const CFStringRef kExtensionIdentifierKey(CFSTR("ExtensionIdentifier"));
const CFStringRef kTeamIdentifierKey(CFSTR("TeamIdentifier"));
const CFStringRef kHostsKey(CFSTR("Hosts"));
const CFStringRef kUrlsKey(CFSTR("URLs"));

using Config = ExtensibleEnterpriseSSOPrefsHandler::Config;

// The handler is owned by `PlatformAuthPolicyObserver` and is created and
// destroyed as the policy changes, so a non-owning pointer is tracked here
// instead of making the class a true singleton.
ExtensibleEnterpriseSSOPrefsHandler* g_instance = nullptr;

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
    return base::BindOnce([]() { return RawConfig(); });
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
      auto urls = ScopedPropList(
          CFPreferencesCopyAppValue(kUrlsKey, kExtensibleSsoPrefName));
      return RawConfig{
          .extension_id = std::move(extension_id),
          .team_id = std::move(team_id),
          .hosts = std::move(hosts),
          .urls = std::move(urls),
      };
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

// Returns std::nullopt if string_prop is nullptr, not CFString, is empty or it
// fails to convert to UTF8.
std::optional<std::string> ParseCfString(CFPropertyListRef string_prop) {
  if (!string_prop) {
    return std::nullopt;
  }

  const CFStringRef cfstring = base::apple::CFCast<CFStringRef>(string_prop);
  if (!cfstring || CFStringGetLength(cfstring) == 0) {
    return std::nullopt;
  }

  std::string string = base::SysCFStringRefToUTF8(cfstring);
  // This can happen despite cfstring not being empty.
  if (string.empty()) {
    return std::nullopt;
  }

  return string;
}

std::optional<std::vector<std::string>> ParseHostnames(
    CFPropertyListRef hostnames_prop) {
  if (!hostnames_prop) {
    return std::nullopt;
  }

  const CFArrayRef array = base::apple::CFCast<CFArrayRef>(hostnames_prop);
  if (!array) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO)
        << "Hosts entry in the SSO extension MDM payload is not an array.";
    return std::nullopt;
  }

  const CFIndex size = CFArrayGetCount(array);
  std::vector<std::string> hostnames;
  hostnames.reserve(size);
  for (CFIndex i = 0; i < size; ++i) {
    std::optional<std::string> hostname =
        ParseCfString(CFArrayGetValueAtIndex(array, i));
    if (hostname.has_value()) {
      hostnames.push_back(std::move(hostname.value()));
    }
  }

  return hostnames;
}

std::optional<std::vector<GURL>> ParseUrls(CFPropertyListRef urls_prop) {
  if (!urls_prop) {
    return std::nullopt;
  }

  const CFArrayRef array = base::apple::CFCast<CFArrayRef>(urls_prop);
  if (!array) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO)
        << "URLs entry in the SSO extension MDM payload is not an array.";
    return std::nullopt;
  }

  const CFIndex size = CFArrayGetCount(array);
  std::vector<GURL> urls;
  urls.reserve(size);
  for (CFIndex i = 0; i < size; ++i) {
    std::optional<std::string> url_str =
        ParseCfString(CFArrayGetValueAtIndex(array, i));
    if (!url_str.has_value() || url_str->empty()) {
      continue;
    }

    // This normalises the url - it must ever only be compared to another GURL.
    GURL url(std::move(url_str).value());

    // Only allow valid HTTP or HTTPS urls without query params, refs or
    // credentials.
    if (url.is_valid() && url.SchemeIsHTTPOrHTTPS() && !url.has_query() &&
        !url.has_ref() && !url.has_username() && !url.has_password()) {
      urls.push_back(std::move(url));
    }
  }

  return urls;
}

std::string OptionalListToString(
    const std::optional<std::vector<std::string>>& list) {
  if (!list.has_value()) {
    return "<missing>";
  }
  return base::StrCat({"[", base::JoinString(list.value(), ", "), "]"});
}

std::string ConfigToString(const Config& config) {
  std::optional<std::vector<std::string>> url_specs;
  if (config.urls.has_value()) {
    url_specs.emplace();
    url_specs->reserve(config.urls->size());
    for (const GURL& url : config.urls.value()) {
      url_specs->push_back(url.spec());
    }
  }

  return base::StrCat({"{extension_id: ", config.extension_id,
                       ", team_id: ", config.team_id,
                       ", hosts: ", OptionalListToString(config.hosts),
                       ", urls: ", OptionalListToString(url_specs), "}"});
}

}  // namespace

ExtensibleEnterpriseSSOPrefsHandler::ExtensibleEnterpriseSSOPrefsHandler(
    PrefService* local_state)
    : cf_preferences_observer_(CreateCfPreferencesObserver()),
      local_state_(local_state),
      read_config_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {
  CHECK(cf_preferences_observer_, base::NotFatalUntil::M161);
  CHECK(local_state_, base::NotFatalUntil::M161);
  CHECK(!g_instance);
  g_instance = this;

  auto callback =
      base::BindRepeating(&ExtensibleEnterpriseSSOPrefsHandler::UpdatePrefs,
                          weak_ptr_factory_.GetWeakPtr());
  auto thread_safe_callback =
      base::BindPostTaskToCurrentDefault(std::move(callback));
  cf_preferences_observer_->Subscribe(std::move(thread_safe_callback));
}

ExtensibleEnterpriseSSOPrefsHandler::~ExtensibleEnterpriseSSOPrefsHandler() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;

  CHECK(cf_preferences_observer_, base::NotFatalUntil::M161);
  cf_preferences_observer_->Unsubscribe();
}

// static
std::optional<ExtensibleEnterpriseSSOPrefsHandler::Config>
ExtensibleEnterpriseSSOPrefsHandler::GetCurrentConfig() {
  if (!g_instance) {
    return std::nullopt;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(g_instance->sequence_checker_);
  return g_instance->current_config_;
}

void ExtensibleEnterpriseSSOPrefsHandler::UpdatePrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(local_state_, base::NotFatalUntil::M161);

  // Read and parse config on a background sequence, the result is sent back to
  // the main thread and handled by OnConfigRead.
  auto parse_callback = base::BindOnce(&ParseConfig);
  auto on_config_parsed =
      base::BindOnce(&ExtensibleEnterpriseSSOPrefsHandler::OnConfigRead,
                     weak_ptr_factory_.GetWeakPtr());
  read_config_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      cf_preferences_observer_->GetReadConfigCallback().Then(
          std::move(parse_callback)),
      std::move(on_config_parsed));
}

void ExtensibleEnterpriseSSOPrefsHandler::OnConfigRead(
    std::optional<Config> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_config_ = std::move(config);

  if (current_config_.has_value()) {
    VLOG_POLICY(1, EXTENSIBLE_SSO) << "Parsed SSO extension MDM payload: "
                                   << ConfigToString(current_config_.value());
  }

  // TODO(b/573090861): Deprecate kExtensibleEnterpriseSSOConfiguredHosts in
  // favour of using GetCurrentConfig().
  if (!current_config_.has_value() || !current_config_->hosts.has_value()) {
    local_state_->SetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts, {});
    return;
  }

  base::ListValue hosts;
  hosts.reserve(current_config_->hosts->size());
  for (const std::string& host : current_config_->hosts.value()) {
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
std::optional<ExtensibleEnterpriseSSOPrefsHandler::Config>
ExtensibleEnterpriseSSOPrefsHandler::ParseConfig(
    CFPreferencesObserver::RawConfig config) {
  if (!config.extension_id || !config.team_id) {
    VLOG_POLICY(1, EXTENSIBLE_SSO)
        << "SSO extension MDM payload not found or incomplete.";
    return std::nullopt;
  }

  // If the extension or team IDs don't match any supported IdPs return an empty
  // result.
  std::optional<std::string> extension_id =
      ParseCfString(config.extension_id.get());
  if (!extension_id.has_value()) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO) << "Failed to parse SSO extension MDM "
                                           "payload: extension_id is invalid.";
    return std::nullopt;
  }

  std::optional<std::string> team_id = ParseCfString(config.team_id.get());
  if (!team_id.has_value()) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO) << "Failed to parse SSO extension MDM "
                                           "payload: team_id is invalid.";
    return std::nullopt;
  }

  // Only Okta is supported for now since request proxying in
  // `PlatformAuthProxyingURLLoaderFactory` only handles Okta SSO requests.
  const SsoExtensionMetadata* metadata =
      FindSsoExtensionMetadata(*team_id, *extension_id);
  if (!metadata || metadata->idp_name != kOktaIdentityProvider) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO)
        << "Failed to parse SSO extension MDM "
           "payload: extension does not match any supported IdP.";
    return std::nullopt;
  }

  std::optional<std::vector<std::string>> hostnames =
      ParseHostnames(config.hosts.get());

  std::optional<std::vector<GURL>> urls = ParseUrls(config.urls.get());

  return ExtensibleEnterpriseSSOPrefsHandler::Config{
      .extension_id = std::move(extension_id).value(),
      .team_id = std::move(team_id).value(),
      .hosts = std::move(hostnames),
      .urls = std::move(urls),
  };
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
