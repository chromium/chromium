// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/net/system_network_context_manager.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/auto_reset.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/crl_set_component_installer.h"
#include "chrome/browser/component_updater/first_party_sets_component_installer.h"
#include "chrome/browser/component_updater/pki_metadata_component_installer.h"
#include "chrome/browser/net/chrome_mojo_proxy_resolver_factory.h"
#include "chrome/browser/net/convert_explicitly_allowed_network_ports_pref.h"
#include "chrome/browser/net/default_dns_over_https_config_source.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ssl/sct_reporting_service.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/google_url_loader_throttle.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/net_log/net_export_file_writer.h"
#include "components/net_log/net_log_proxy_source.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/network_context_client_base.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/features.h"
#include "net/cookies/cookie_util.h"
#include "net/net_buildflags.h"
#include "net/third_party/uri_template/uri_template.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/sandbox_type.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_annotation_monitor.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/net/network_annotation_monitor.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/net/dhcp_wpad_url_client.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process_platform_part.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/net/chrome_mojo_proxy_resolver_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {
// Enumeration of possible sandbox states. These values are persisted to logs,
// so entries should not be renumbered and numeric values should never be
// reused.
enum class NetworkSandboxState {
  // Disabled by platform configuration. Either the platform does not support
  // it, or the feature is disabled.
  kDisabledByPlatform = 0,
  // Enabled by platform configuration. Either the platform has it enabled by
  // default, or it's enabled by feature.
  kEnabledByPlatform = 1,
  // Enabled by policy. Only valid on Windows where the policy is respected.
  kDisabledByPolicy = 2,
  // Disabled by policy. Only valid on Windows where the policy is respected.
  kEnabledByPolicy = 3,
  // Disabled because of a previous failed launch attempt.
  kDisabledBecauseOfFailedLaunch = 4,
  // Disabled because the user (might) want kerberos, which is incompatible with
  // the Linux/Cros sandbox.
  kDisabledBecauseOfKerberos = 5,
  kMaxValue = kDisabledBecauseOfKerberos
};

// The global instance of the SystemNetworkContextManager.
SystemNetworkContextManager* g_system_network_context_manager = nullptr;

// Whether or not any instance of the system network context manager has
// received a failed launch for a sandboxed network service.
bool g_previously_failed_to_launch_sandboxed_service = false;

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// Whether kerberos library loading will work in the network service due to the
// sandbox.
bool g_network_service_will_allow_gssapi_library_load = false;

const char* kGssapiDesiredPref =
#if BUILDFLAG(IS_CHROMEOS)
    prefs::kKerberosEnabled;
#elif BUILDFLAG(IS_LINUX)
    prefs::kReceivedHttpAuthNegotiateHeader;
#endif
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

// Constructs HttpAuthStaticParams based on |local_state|.
network::mojom::HttpAuthStaticParamsPtr CreateHttpAuthStaticParams(
    PrefService* local_state) {
  network::mojom::HttpAuthStaticParamsPtr auth_static_params =
      network::mojom::HttpAuthStaticParams::New();

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  auth_static_params->gssapi_library_name =
      local_state->GetString(prefs::kGSSAPILibraryName);
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) &&
        // !BUILDFLAG(IS_CHROMEOS)

  return auth_static_params;
}

// Constructs HttpAuthDynamicParams based on |local_state|.
network::mojom::HttpAuthDynamicParamsPtr CreateHttpAuthDynamicParams(
    PrefService* local_state) {
  network::mojom::HttpAuthDynamicParamsPtr auth_dynamic_params =
      network::mojom::HttpAuthDynamicParams::New();

  auth_dynamic_params->allowed_schemes =
      base::SplitString(local_state->GetString(prefs::kAuthSchemes), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (const base::Value& item :
       local_state->GetList(prefs::kAllHttpAuthSchemesAllowedForOrigins)) {
    auth_dynamic_params->patterns_allowed_to_use_all_schemes.push_back(
        item.GetString());
  }
  auth_dynamic_params->server_allowlist =
      local_state->GetString(prefs::kAuthServerAllowlist);
  auth_dynamic_params->delegate_allowlist =
      local_state->GetString(prefs::kAuthNegotiateDelegateAllowlist);
  auth_dynamic_params->negotiate_disable_cname_lookup =
      local_state->GetBoolean(prefs::kDisableAuthNegotiateCnameLookup);
  auth_dynamic_params->enable_negotiate_port =
      local_state->GetBoolean(prefs::kEnableAuthNegotiatePort);
  auth_dynamic_params->basic_over_http_enabled =
      local_state->GetBoolean(prefs::kBasicAuthOverHttpEnabled);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  auth_dynamic_params->delegate_by_kdc_policy =
      local_state->GetBoolean(prefs::kAuthNegotiateDelegateByKdcPolicy);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_POSIX)
  auth_dynamic_params->ntlm_v2_enabled =
      local_state->GetBoolean(prefs::kNtlmV2Enabled);
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_ANDROID)
  auth_dynamic_params->android_negotiate_account_type =
      local_state->GetString(prefs::kAuthAndroidNegotiateAccountType);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  auth_dynamic_params->allow_gssapi_library_load =
      local_state->GetBoolean(kGssapiDesiredPref);
#endif  // BUILDFLAG(IS_CHROMEOS)

  return auth_dynamic_params;
}

void OnNewHttpAuthDynamicParams(
    network::mojom::HttpAuthDynamicParamsPtr& params) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // The kerberos library is incompatible with the network service sandbox, so
  // if library loading is now enabled, the network service needs to be
  // restarted. It will be restarted unsandboxed because is
  // `g_network_service_will_allow_gssapi_library_load` will be set.
  if (params->allow_gssapi_library_load &&
      !g_network_service_will_allow_gssapi_library_load) {
    g_network_service_will_allow_gssapi_library_load = true;
    // The network service, if sandboxed, will still not allow gssapi library
    // loading until it is shut down. On restart the network service will get
    // the correct value.
    // NOTE: technically another call to OnNewHttpAuthDynamicParams() before the
    // RestartNetworkService() call will leave this as true. This could happen
    // if the admin sends out another edit to the HTTP auth dynamic params right
    // after enabling kerberos. Then if the user attempts a load of a page that
    // requires "negotiate" HTTP auth, the network service will load a GSSAPI
    // library despite the sandbox. This is incredibly unlikely, probably
    // wouldn't have any consequences anyway, and would be quickly self-healing
    // once RestartNetworkService() runs, so there's no reason to handle this
    // case.
    params->allow_gssapi_library_load = false;
    // Post a shutdown task because the current task probably holds a raw
    // pointer to the remote.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&content::RestartNetworkService));
  }
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
}

void OnAuthPrefsChanged(PrefService* local_state,
                        const std::string& pref_name) {
  auto params = CreateHttpAuthDynamicParams(local_state);
  OnNewHttpAuthDynamicParams(params);
  content::GetNetworkService()->ConfigureHttpAuthPrefs(std::move(params));
}

NetworkSandboxState IsNetworkSandboxEnabledInternal() {
  // If previously an attempt to launch the sandboxed process failed, then
  // launch unsandboxed.
  if (g_previously_failed_to_launch_sandboxed_service) {
    return NetworkSandboxState::kDisabledBecauseOfFailedLaunch;
  }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  auto* local_state = g_browser_process->local_state();
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // The network service sandbox and the kerberos library are incompatible.
  // If kerberos is enabled by policy, disable the network service sandbox.
  if (g_network_service_will_allow_gssapi_library_load ||
      (local_state && local_state->HasPrefPath(kGssapiDesiredPref) &&
       local_state->GetBoolean(kGssapiDesiredPref))) {
    return NetworkSandboxState::kDisabledBecauseOfKerberos;
  }
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
  if (!sandbox::policy::features::IsNetworkSandboxSupported()) {
    return NetworkSandboxState::kDisabledByPlatform;
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  if (local_state &&
      local_state->HasPrefPath(prefs::kNetworkServiceSandboxEnabled)) {
    return local_state->GetBoolean(prefs::kNetworkServiceSandboxEnabled)
               ? NetworkSandboxState::kEnabledByPolicy
               : NetworkSandboxState::kDisabledByPolicy;
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

  // If no policy is specified, then delegate to global sandbox configuration.
  return sandbox::policy::features::IsNetworkSandboxEnabled()
             ? NetworkSandboxState::kEnabledByPlatform
             : NetworkSandboxState::kDisabledByPlatform;
}

std::vector<network::mojom::CTLogInfoPtr> GetStaticCtLogListMojo() {
  std::vector<std::pair<std::string, base::Time>> disqualified_logs =
      certificate_transparency::GetDisqualifiedLogs();
  std::vector<network::mojom::CTLogInfoPtr> log_list_mojo;
  for (const auto& ct_log : certificate_transparency::GetKnownLogs()) {
    network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
    log_info->public_key = std::string(ct_log.log_key, ct_log.log_key_length);
    log_info->id = crypto::SHA256HashString(log_info->public_key);
    log_info->name = ct_log.log_name;
    log_info->current_operator = ct_log.current_operator;

    auto it = std::lower_bound(
        std::begin(disqualified_logs), std::end(disqualified_logs),
        log_info->id,
        [](const auto& disqualified_log, const std::string& log_id) {
          return disqualified_log.first < log_id;
        });
    if (it != std::end(disqualified_logs) && it->first == log_info->id) {
      log_info->disqualified_at = it->second;
    }

    for (size_t i = 0; i < ct_log.previous_operators_length; i++) {
      const auto& op = ct_log.previous_operators[i];
      network::mojom::PreviousOperatorEntryPtr previous_operator =
          network::mojom::PreviousOperatorEntry::New();
      previous_operator->name = op.name;
      previous_operator->end_time = op.end_time;
      log_info->previous_operators.push_back(std::move(previous_operator));
    }

    log_list_mojo.push_back(std::move(log_info));
  }
  return log_list_mojo;
}

}  // namespace

class SystemNetworkContextManager::NetworkProcessLaunchWatcher
    : public content::BrowserChildProcessObserver {
 public:
  NetworkProcessLaunchWatcher() { BrowserChildProcessObserver::Add(this); }

  NetworkProcessLaunchWatcher(const NetworkProcessLaunchWatcher&) = delete;
  NetworkProcessLaunchWatcher& operator=(const NetworkProcessLaunchWatcher&) =
      delete;

  ~NetworkProcessLaunchWatcher() override {
    BrowserChildProcessObserver::Remove(this);
  }

 private:
  void BrowserChildProcessLaunchFailed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override {
    if (data.sandbox_type == sandbox::mojom::Sandbox::kNetwork) {
      // This histogram duplicates data recorded in
      // ChildProcess.LaunchFailed.UtilityProcessErrorCode but is specific to
      // the network service to make analysis easier.
      base::UmaHistogramSparse(
          "Chrome.SystemNetworkContextManager.NetworkSandboxLaunchFailed."
          "ErrorCode",
          info.exit_code);
#if BUILDFLAG(IS_WIN)
      // This histogram duplicates data recorded in
      // ChildProcess.LaunchFailed.WinLastError but is specific to the network
      // service to make analysis easier.
      base::UmaHistogramSparse(
          "Chrome.SystemNetworkContextManager.NetworkSandboxLaunchFailed."
          "WinLastError",
          info.last_error);
#endif  // BUILDFLAG(IS_WIN)
      g_previously_failed_to_launch_sandboxed_service = true;
    }
  }
};

// SharedURLLoaderFactory backed by a SystemNetworkContextManager and its
// network context. Transparently handles crashes.
class SystemNetworkContextManager::URLLoaderFactoryForSystem
    : public network::SharedURLLoaderFactory {
 public:
  explicit URLLoaderFactoryForSystem(SystemNetworkContextManager* manager)
      : manager_(manager) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  URLLoaderFactoryForSystem(const URLLoaderFactoryForSystem&) = delete;
  URLLoaderFactoryForSystem& operator=(const URLLoaderFactoryForSystem&) =
      delete;

  // mojom::URLLoaderFactory implementation:

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!manager_)
      return;
    manager_->GetURLLoaderFactory()->CreateLoaderAndStart(
        std::move(receiver), request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    if (!manager_)
      return;
    manager_->GetURLLoaderFactory()->Clone(std::move(receiver));
  }

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
        this);
  }

  void Shutdown() { manager_ = nullptr; }

 private:
  friend class base::RefCounted<URLLoaderFactoryForSystem>;
  ~URLLoaderFactoryForSystem() override = default;

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<SystemNetworkContextManager> manager_;
};

network::mojom::NetworkContext* SystemNetworkContextManager::GetContext() {
  if (!network_service_network_context_ ||
      !network_service_network_context_.is_connected()) {
    // This should call into OnNetworkServiceCreated(), which will re-create
    // the network service, if needed. There's a chance that it won't be
    // invoked, if the NetworkContext has encountered an error but the
    // NetworkService has not yet noticed its pipe was closed. In that case,
    // trying to create a new NetworkContext would fail, anyways, and hopefully
    // a new NetworkContext will be created on the next GetContext() call.
    content::GetNetworkService();
    DCHECK(network_service_network_context_);
  }
  return network_service_network_context_.get();
}

network::mojom::URLLoaderFactory*
SystemNetworkContextManager::GetURLLoaderFactory() {
  // Create the URLLoaderFactory as needed.
  if (url_loader_factory_ && url_loader_factory_.is_connected()) {
    return url_loader_factory_.get();
  }

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_orb_enabled = false;
  params->is_trusted = true;

  url_loader_factory_.reset();
  GetContext()->CreateURLLoaderFactory(
      url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));
  return url_loader_factory_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
SystemNetworkContextManager::GetSharedURLLoaderFactory() {
  return shared_url_loader_factory_;
}

// static
SystemNetworkContextManager* SystemNetworkContextManager::CreateInstance(
    PrefService* pref_service) {
#if DCHECK_IS_ON()
  // Check that this function is not reentrant.
  static bool inside_this_function = false;
  DCHECK(!inside_this_function);
  base::AutoReset now_inside_this_function(&inside_this_function, true);
#endif  // DCHECK_IS_ON()

  DCHECK(!g_system_network_context_manager);
  g_system_network_context_manager =
      new SystemNetworkContextManager(pref_service);
  return g_system_network_context_manager;
}

// static
bool SystemNetworkContextManager::HasInstance() {
  return !!g_system_network_context_manager;
}

// static
SystemNetworkContextManager* SystemNetworkContextManager::GetInstance() {
  if (!g_system_network_context_manager) {
    // Initialize the network service, which will trigger
    // ChromeContentBrowserClient::OnNetworkServiceCreated(), which calls
    // CreateInstance() to initialize |g_system_network_context_manager|.
    content::GetNetworkService();

    // TODO(crbug.com/40634772): There should be a DCHECK() here to make sure
    // |g_system_network_context_manager| has been created, but that is not
    // true in many unit tests.
  }

  return g_system_network_context_manager;
}

// static
void SystemNetworkContextManager::DeleteInstance() {
  DCHECK(g_system_network_context_manager);
  delete g_system_network_context_manager;
  g_system_network_context_manager = nullptr;
}

#if BUILDFLAG(IS_LINUX)
SystemNetworkContextManager::GssapiLibraryLoadObserver::
    GssapiLibraryLoadObserver(SystemNetworkContextManager* owner)
    : owner_(owner) {}

SystemNetworkContextManager::GssapiLibraryLoadObserver::
    ~GssapiLibraryLoadObserver() = default;

void SystemNetworkContextManager::GssapiLibraryLoadObserver::Install(
    network::mojom::NetworkService* network_service) {
  gssapi_library_loader_observer_receiver_.reset();
  network_service->SetGssapiLibraryLoadObserver(
      gssapi_library_loader_observer_receiver_.BindNewPipeAndPassRemote());
}

void SystemNetworkContextManager::GssapiLibraryLoadObserver::
    OnBeforeGssapiLibraryLoad() {
  owner_->local_state_->SetBoolean(prefs::kReceivedHttpAuthNegotiateHeader,
                                   true);
}
#endif  // BUILDFLAG(IS_LINUX)

SystemNetworkContextManager::SystemNetworkContextManager(
    PrefService* local_state)
    : local_state_(local_state),
      ssl_config_service_manager_(local_state_),
      proxy_config_monitor_(local_state_),
      stub_resolver_config_reader_(local_state_) {
#if !BUILDFLAG(IS_ANDROID)
  // QuicAllowed was not part of Android policy.
  const base::Value* value =
      g_browser_process->policy_service()
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()))
          .GetValue(policy::key::kQuicAllowed, base::Value::Type::BOOLEAN);
  if (value)
    is_quic_allowed_ = value->GetBool();
#endif  // !BUILDFLAG(IS_ANDROID)
  shared_url_loader_factory_ = new URLLoaderFactoryForSystem(this);

  pref_change_registrar_.Init(local_state_);

  PrefChangeRegistrar::NamedChangeCallback auth_pref_callback =
      base::BindRepeating(&OnAuthPrefsChanged, base::Unretained(local_state_));

  pref_change_registrar_.Add(prefs::kAuthSchemes, auth_pref_callback);
  pref_change_registrar_.Add(prefs::kAuthServerAllowlist, auth_pref_callback);
  pref_change_registrar_.Add(prefs::kAuthNegotiateDelegateAllowlist,
                             auth_pref_callback);
  pref_change_registrar_.Add(prefs::kDisableAuthNegotiateCnameLookup,
                             auth_pref_callback);
  pref_change_registrar_.Add(prefs::kEnableAuthNegotiatePort,
                             auth_pref_callback);
  pref_change_registrar_.Add(prefs::kBasicAuthOverHttpEnabled,
                             auth_pref_callback);
  pref_change_registrar_.Add(prefs::kAllHttpAuthSchemesAllowedForOrigins,
                             auth_pref_callback);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  pref_change_registrar_.Add(prefs::kAuthNegotiateDelegateByKdcPolicy,
                             auth_pref_callback);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_POSIX)
  pref_change_registrar_.Add(prefs::kNtlmV2Enabled, auth_pref_callback);
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_ANDROID)
  pref_change_registrar_.Add(prefs::kAuthAndroidNegotiateAccountType,
                             auth_pref_callback);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  pref_change_registrar_.Add(kGssapiDesiredPref, auth_pref_callback);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

  local_state_->SetDefaultPrefValue(
      prefs::kEnableReferrers,
      base::Value(!base::FeatureList::IsEnabled(features::kNoReferrers)));
  enable_referrers_.Init(
      prefs::kEnableReferrers, local_state_,
      base::BindRepeating(&SystemNetworkContextManager::UpdateReferrersEnabled,
                          base::Unretained(this)));

  pref_change_registrar_.Add(
      prefs::kExplicitlyAllowedNetworkPorts,
      base::BindRepeating(
          &SystemNetworkContextManager::UpdateExplicitlyAllowedNetworkPorts,
          base::Unretained(this)));

#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
  // TODO(crbug.com/40941700): If this call is removed, clank crashes on
  // startup. Not sure why.
  content::GetCertVerifierServiceFactory()->SetUseChromeRootStore(
      true, base::DoNothing());
#endif

  if (content::IsOutOfProcessNetworkService() &&
      base::FeatureList::IsEnabled(
          features::kRestartNetworkServiceUnsandboxedForFailedLaunch)) {
    network_process_launch_watcher_ =
        std::make_unique<NetworkProcessLaunchWatcher>();
  }

  pref_change_registrar_.Add(
      prefs::kIPv6ReachabilityOverrideEnabled,
      base::BindRepeating(
          &SystemNetworkContextManager::UpdateIPv6ReachabilityOverrideEnabled,
          base::Unretained(this)));
}

SystemNetworkContextManager::~SystemNetworkContextManager() {
  shared_url_loader_factory_->Shutdown();
}

// static
void SystemNetworkContextManager::RegisterPrefs(PrefRegistrySimple* registry) {
  StubResolverConfigReader::RegisterPrefs(registry);
  DefaultDnsOverHttpsConfigSource::RegisterPrefs(registry);

  // Static auth params
  registry->RegisterStringPref(prefs::kAuthSchemes,
                               "basic,digest,ntlm,negotiate");
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  registry->RegisterStringPref(prefs::kGSSAPILibraryName, std::string());
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) &&
        // !BUILDFLAG(IS_CHROMEOS)

  // Dynamic auth params.
  registry->RegisterListPref(prefs::kAllHttpAuthSchemesAllowedForOrigins);
  registry->RegisterBooleanPref(prefs::kDisableAuthNegotiateCnameLookup, false);
  registry->RegisterBooleanPref(prefs::kEnableAuthNegotiatePort, false);
  registry->RegisterBooleanPref(prefs::kBasicAuthOverHttpEnabled, true);
  registry->RegisterStringPref(prefs::kAuthServerAllowlist, std::string());
  registry->RegisterStringPref(prefs::kAuthNegotiateDelegateAllowlist,
                               std::string());

// On ChromeOS Ash, the pref below is registered by the
// `KerberosCredentialsManager`.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  registry->RegisterBooleanPref(prefs::kKerberosEnabled, false);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kAuthNegotiateDelegateByKdcPolicy,
                                false);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_POSIX)
  registry->RegisterBooleanPref(prefs::kNtlmV2Enabled, true);
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterStringPref(prefs::kAuthAndroidNegotiateAccountType,
                               std::string());
#endif  // BUILDFLAG(IS_ANDROID)

  // Per-NetworkContext pref. The pref value from |local_state_| is used for
  // the system NetworkContext, and the per-profile pref values are used for
  // the profile NetworkContexts.
  registry->RegisterBooleanPref(prefs::kEnableReferrers, true);

  registry->RegisterBooleanPref(prefs::kQuickCheckEnabled, true);

  registry->RegisterIntegerPref(prefs::kMaxConnectionsPerProxy, -1);

  registry->RegisterListPref(prefs::kExplicitlyAllowedNetworkPorts);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  registry->RegisterBooleanPref(prefs::kNetworkServiceSandboxEnabled, true);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX)
  registry->RegisterBooleanPref(prefs::kReceivedHttpAuthNegotiateHeader, false);
#endif  // BUILDFLAG(IS_LINUX)

  registry->RegisterBooleanPref(prefs::kZstdContentEncodingEnabled, true);

  registry->RegisterBooleanPref(prefs::kIPv6ReachabilityOverrideEnabled, false);
}

// static
StubResolverConfigReader*
SystemNetworkContextManager::GetStubResolverConfigReader() {
  if (stub_resolver_config_reader_for_testing_)
    return stub_resolver_config_reader_for_testing_;

  return &GetInstance()->stub_resolver_config_reader_;
}

void SystemNetworkContextManager::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  // On network service restart, it's possible for |url_loader_factory_| to not
  // be disconnected yet (so any consumers of GetURLLoaderFactory() in network
  // service restart handling code could end up getting the old factory, which
  // will then get disconnected later). Resetting the Remote is a no-op for the
  // initial creation of the network service, but for restarts this guarantees
  // that GetURLLoaderFactory() works as expected.
  // (See crbug.com/1131803 for a motivating example and investigation.)
  url_loader_factory_.reset();

  // Disable QUIC globally, if needed.
  if (!is_quic_allowed_)
    network_service->DisableQuic();

  if (content::IsOutOfProcessNetworkService()) {
    mojo::PendingRemote<network::mojom::NetLogProxySource> proxy_source_remote;
    mojo::PendingReceiver<network::mojom::NetLogProxySource>
        proxy_source_receiver =
            proxy_source_remote.InitWithNewPipeAndPassReceiver();
    mojo::Remote<network::mojom::NetLogProxySink> proxy_sink_remote;
    network_service->AttachNetLogProxy(
        std::move(proxy_source_remote),
        proxy_sink_remote.BindNewPipeAndPassReceiver());
    if (net_log_proxy_source_)
      net_log_proxy_source_->ShutDown();
    net_log_proxy_source_ = std::make_unique<net_log::NetLogProxySource>(
        std::move(proxy_source_receiver), std::move(proxy_sink_remote));
  }

  network_service->SetUpHttpAuth(CreateHttpAuthStaticParams(local_state_));
  auto http_auth_dynamic_params = CreateHttpAuthDynamicParams(local_state_);
  OnNewHttpAuthDynamicParams(http_auth_dynamic_params);
  network_service->ConfigureHttpAuthPrefs(std::move(http_auth_dynamic_params));

#if BUILDFLAG(IS_LINUX)
  gssapi_library_loader_observer_.Install(network_service);
#endif  // BUILDFLAG(IS_LINUX)

  // Configure the static Certificate Transparency logs. This must be done
  // before the PKIMetadataComponentInstallerService
  // ReconfigureAfterNetworkRestart call below.
  if (IsCertificateTransparencyEnabled()) {
    content::GetCertVerifierServiceFactory()->UpdateCtLogList(
        GetStaticCtLogListMojo(),
        certificate_transparency::GetLogListTimestamp(), base::DoNothing());
    network_service->UpdateCtLogList(GetStaticCtLogListMojo(),
                                     base::DoNothing());
  }

  int max_connections_per_proxy =
      local_state_->GetInteger(prefs::kMaxConnectionsPerProxy);
  if (max_connections_per_proxy != -1) {
    network_service->SetMaxConnectionsPerProxyChain(max_connections_per_proxy);
  }

  network_service_network_context_.reset();
  content::CreateNetworkContextInNetworkService(
      network_service_network_context_.BindNewPipeAndPassReceiver(),
      CreateNetworkContextParams());

  mojo::PendingRemote<network::mojom::NetworkContextClient> client_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<content::NetworkContextClientBase>(),
      client_remote.InitWithNewPipeAndPassReceiver());
  network_service_network_context_->SetClient(std::move(client_remote));

  // Configure the stub resolver. This must be done after the system
  // NetworkContext is created, but before anything has the chance to use it.
  stub_resolver_config_reader_.UpdateNetworkService(true /* record_metrics */);

  // The OSCrypt keys are process bound, so if network service is out of
  // process, send it the required key.
  if (content::IsOutOfProcessNetworkService()) {
    // On Windows, OSCrypt Async manages the encryption key via the DPAPI key
    // provider, and there is no need to send the key separately to OSCrypt
    // sync.
#if !BUILDFLAG(IS_WIN)
    network_service->SetEncryptionKey(OSCrypt::GetRawEncryptionKey());
#endif  // !BUILDFLAG(IS_WIN)
  }

  // Configure SCT Auditing in the NetworkService.
  SCTReportingService::ReconfigureAfterNetworkRestart();

  component_updater::PKIMetadataComponentInstallerService::GetInstance()
      ->ReconfigureAfterNetworkRestart();

  UpdateExplicitlyAllowedNetworkPorts();

  UpdateIPv6ReachabilityOverrideEnabled();

#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kNetworkAnnotationMonitoring)) {
    // Create NetworkAnnotationMonitor.
    if (!network_annotation_monitor_) {
      network_annotation_monitor_ =
          std::make_unique<NetworkAnnotationMonitor>();
    }

    // Pass NetworkAnnotationMonitor remote to NetworkService so that network
    // calls can be reported.
    network_service->SetNetworkAnnotationMonitor(
        network_annotation_monitor_->GetClient());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // cert_verifier_time_updater_ does not depend on the network service, but
  // can't be initialized from the constructor since network_time_tracker()
  // requires the SystemNetworkContextManager to be ready. It does not need to
  // be recreated every time the network service is restarted (and should not,
  // since it expects to outlive the NetworkTimeTracker).
  // g_browser_process can be null in some tests.
  if (base::FeatureList::IsEnabled(features::kCertVerificationNetworkTime) &&
      !cert_verifier_time_updater_ && g_browser_process) {
    cert_verifier_time_updater_ =
        std::make_unique<CertVerifierServiceTimeUpdater>(
            g_browser_process->network_time_tracker());
  }
}

void SystemNetworkContextManager::DisableQuic() {
  is_quic_allowed_ = false;

  // Disabling QUIC for a profile disables QUIC globally. As a side effect, new
  // Profiles will also have QUIC disabled because the network service will
  // disable QUIC.
  content::GetNetworkService()->DisableQuic();
}

void SystemNetworkContextManager::
    AddCookieEncryptionManagerToNetworkContextParams(
        network::mojom::NetworkContextParams* network_context_params) {
  network_context_params->cookie_encryption_provider =
      cookie_encryption_provider_.BindNewRemote();
}

void SystemNetworkContextManager::AddSSLConfigToNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params) {
  ssl_config_service_manager_.AddToNetworkContextParams(network_context_params);
}

void SystemNetworkContextManager::ConfigureDefaultNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params) {
  variations::UpdateCorsExemptHeaderForVariations(network_context_params);
  GoogleURLLoaderThrottle::UpdateCorsExemptHeader(network_context_params);

  network_context_params->enable_brotli = true;

  network_context_params->enable_zstd =
      base::FeatureList::IsEnabled(net::features::kZstdContentEncoding) &&
      local_state_->GetBoolean(prefs::kZstdContentEncodingEnabled);

  network_context_params->user_agent = embedder_support::GetUserAgent();

  // Disable referrers by default. Any consumer that enables referrers should
  // respect prefs::kEnableReferrers from the appropriate pref store.
  network_context_params->enable_referrers = false;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // TODO(eroman): Figure out why this doesn't work in single-process mode,
  // or if it does work, now.
  // Should be possible now that a private isolate is used.
  // http://crbug.com/474654
  if (!command_line.HasSwitch(switches::kWinHttpProxyResolver)) {
    if (command_line.HasSwitch(switches::kSingleProcess)) {
      LOG(ERROR) << "Cannot use V8 Proxy resolver in single process mode.";
    } else {
      network_context_params->proxy_resolver_factory =
          ChromeMojoProxyResolverFactory::CreateWithSelfOwnedReceiver();

#if BUILDFLAG(IS_CHROMEOS_ASH)
      network_context_params->dhcp_wpad_url_client =
          ash::DhcpWpadUrlClient::CreateWithSelfOwnedReceiver();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    }
  }

#if BUILDFLAG(IS_WIN)
  if (command_line.HasSwitch(switches::kUseSystemProxyResolver)) {
    network_context_params->windows_system_proxy_resolver =
        ChromeMojoProxyResolverWin::CreateWithSelfOwnedReceiver();
  }
#endif  // BUILDFLAG(IS_WIN)

  network_context_params->pac_quick_check_enabled =
      local_state_->GetBoolean(prefs::kQuickCheckEnabled);

  // Use the SystemNetworkContextManager to populate and update SSL
  // configuration. The SystemNetworkContextManager is owned by the
  // BrowserProcess itself, so will only be destroyed on shutdown, at which
  // point, all NetworkContexts will be destroyed as well.
  AddSSLConfigToNetworkContextParams(network_context_params);

  if (IsCertificateTransparencyEnabled()) {
    network_context_params->enforce_chrome_ct_policy = true;
  }
}

network::mojom::NetworkContextParamsPtr
SystemNetworkContextManager::CreateDefaultNetworkContextParams() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  cert_verifier::mojom::CertVerifierCreationParamsPtr
      cert_verifier_creation_params =
          cert_verifier::mojom::CertVerifierCreationParams::New();
  ConfigureDefaultNetworkContextParams(network_context_params.get());
  // The system network context doesn't update the CertVerifyProc
  // InstanceParams while running, so it does not attach a
  // CertVerifierServiceUpdater.
  network_context_params->cert_verifier_params =
      content::GetCertVerifierParams(std::move(cert_verifier_creation_params));
  network_context_params->acam_preflight_spec_conformant =
      base::FeatureList::IsEnabled(
          network::features::
              kAccessControlAllowMethodsInCORSPreflightSpecConformant);
  return network_context_params;
}

net_log::NetExportFileWriter*
SystemNetworkContextManager::GetNetExportFileWriter() {
  if (!net_export_file_writer_) {
    net_export_file_writer_ = std::make_unique<net_log::NetExportFileWriter>();
  }
  return net_export_file_writer_.get();
}

// static
bool SystemNetworkContextManager::IsNetworkSandboxEnabled() {
  NetworkSandboxState state = IsNetworkSandboxEnabledInternal();

  base::UmaHistogramEnumeration(
      "Chrome.SystemNetworkContextManager.NetworkSandboxState", state);

  bool enabled = true;
  switch (state) {
    case NetworkSandboxState::kDisabledBecauseOfKerberos:
      enabled = false;
      break;
    case NetworkSandboxState::kDisabledByPlatform:
      enabled = false;
      break;
    case NetworkSandboxState::kEnabledByPlatform:
      enabled = true;
      break;
    case NetworkSandboxState::kDisabledByPolicy:
      enabled = false;
      break;
    case NetworkSandboxState::kEnabledByPolicy:
      enabled = true;
      break;
    case NetworkSandboxState::kDisabledBecauseOfFailedLaunch:
      enabled = false;
      break;
  }

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  if (!enabled) {
    g_network_service_will_allow_gssapi_library_load = true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

  return enabled;
}

void SystemNetworkContextManager::FlushSSLConfigManagerForTesting() {
  ssl_config_service_manager_.FlushForTesting();
}

void SystemNetworkContextManager::FlushProxyConfigMonitorForTesting() {
  proxy_config_monitor_.FlushForTesting();
}

void SystemNetworkContextManager::FlushNetworkInterfaceForTesting() {
  DCHECK(network_service_network_context_);
  network_service_network_context_.FlushForTesting();
  if (url_loader_factory_)
    url_loader_factory_.FlushForTesting();
}

#if BUILDFLAG(IS_CHROMEOS)
void SystemNetworkContextManager::FlushNetworkAnnotationMonitorForTesting() {
  if (network_annotation_monitor_) {
    network_annotation_monitor_->FlushForTesting();  // IN-TEST
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

network::mojom::HttpAuthStaticParamsPtr
SystemNetworkContextManager::GetHttpAuthStaticParamsForTesting() {
  return CreateHttpAuthStaticParams(g_browser_process->local_state());
}

network::mojom::HttpAuthDynamicParamsPtr
SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting() {
  return CreateHttpAuthDynamicParams(g_browser_process->local_state());
}

void SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
    std::optional<bool> enabled) {
  certificate_transparency_enabled_for_testing_ = enabled;
}

void SystemNetworkContextManager::SetCTLogListTimelyForTesting() {
  content::GetCertVerifierServiceFactory()->UpdateCtLogList(
      GetStaticCtLogListMojo(), base::Time::Now(), base::DoNothing());
}

bool SystemNetworkContextManager::IsCertificateTransparencyEnabled() {
  if (certificate_transparency_enabled_for_testing_.has_value())
    return certificate_transparency_enabled_for_testing_.value();
  // Certificate Transparency is enabled:
  //   - by default for Chrome-branded builds
  //   - on an opt-in basis for other builds and embedders, controlled with the
  //     kCertificateTransparencyAskBeforeEnabling flag
  return base::FeatureList::IsEnabled(
      features::kCertificateTransparencyAskBeforeEnabling);
}

network::mojom::NetworkContextParamsPtr
SystemNetworkContextManager::CreateNetworkContextParams() {
  // TODO(mmenke): Set up parameters here (in memory cookie store, etc).
  network::mojom::NetworkContextParamsPtr network_context_params =
      CreateDefaultNetworkContextParams();

  network_context_params->enable_referrers = enable_referrers_.GetValue();

  network_context_params->http_cache_enabled = false;

  proxy_config_monitor_.AddToNetworkContextParams(network_context_params.get());

  return network_context_params;
}

void SystemNetworkContextManager::UpdateExplicitlyAllowedNetworkPorts() {
  // Currently there are no uses of net::IsPortAllowedForScheme() in the browser
  // process. If someone adds one then we'll have to also call
  // net::SetExplicitlyAllowedPorts() directly here, on the appropriate thread.
  content::GetNetworkService()->SetExplicitlyAllowedPorts(
      ConvertExplicitlyAllowedNetworkPortsPref(local_state_));
}

void SystemNetworkContextManager::UpdateReferrersEnabled() {
  GetContext()->SetEnableReferrers(enable_referrers_.GetValue());
}

void SystemNetworkContextManager::UpdateIPv6ReachabilityOverrideEnabled() {
  bool is_managed = local_state_->IsManagedPreference(
      prefs::kIPv6ReachabilityOverrideEnabled);
  bool pref_value =
      local_state_->GetBoolean(prefs::kIPv6ReachabilityOverrideEnabled);
  bool is_launched = base::FeatureList::IsEnabled(
      net::features::kEnableIPv6ReachabilityOverride);
  bool value = is_managed ? pref_value : is_launched;
  content::GetNetworkService()->SetIPv6ReachabilityOverride(value);
}

// static
StubResolverConfigReader*
    SystemNetworkContextManager::stub_resolver_config_reader_for_testing_ =
        nullptr;

std::optional<bool>
    SystemNetworkContextManager::certificate_transparency_enabled_for_testing_ =
        std::nullopt;
