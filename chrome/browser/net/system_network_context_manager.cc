// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/system_network_context_manager.h"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/component_updater/crl_set_component_installer.h"
#include "chrome/browser/net/chrome_mojo_proxy_resolver_factory.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/net_log/net_export_file_writer.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/os_crypt/os_crypt.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cors_exempt_headers.h"
#include "content/public/browser/network_context_client_base.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mime_handler_view_mode.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/user_agent.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/features.h"
#include "net/net_buildflags.h"
#include "net/third_party/uri_template/uri_template.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/cross_thread_shared_url_loader_factory_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/net/dhcp_wpad_url_client.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "content/public/common/network_service_util.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

constexpr bool kCertificateTransparencyEnabled =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OFFICIAL_BUILD) && \
    !defined(OS_ANDROID)
    // Certificate Transparency is only enabled if:
    //   - Desktop (!OS_ANDROID); OS_IOS does not use this file
    //   - base::GetBuildTime() is deterministic to the source (OFFICIAL_BUILD)
    //   - The build in reliably updatable (GOOGLE_CHROME_BRANDING)
    true;
#else
    false;
#endif

bool g_enable_certificate_transparency = kCertificateTransparencyEnabled;

// The global instance of the SystemNetworkContextmanager.
SystemNetworkContextManager* g_system_network_context_manager = nullptr;

void GetStubResolverConfig(
    PrefService* local_state,
    bool* insecure_stub_resolver_enabled,
    net::DnsConfig::SecureDnsMode* secure_dns_mode,
    base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>*
        dns_over_https_servers) {
  DCHECK(!dns_over_https_servers->has_value());

  *insecure_stub_resolver_enabled =
      local_state->GetBoolean(prefs::kBuiltInDnsClientEnabled);

  std::string doh_mode;
  if (!local_state->FindPreference(prefs::kDnsOverHttpsMode)->IsManaged() &&
      chrome_browser_net::ShouldDisableDohForManaged())
    doh_mode = chrome_browser_net::kDnsOverHttpsModeOff;
  else
    doh_mode = local_state->GetString(prefs::kDnsOverHttpsMode);

  if (doh_mode == chrome_browser_net::kDnsOverHttpsModeSecure)
    *secure_dns_mode = net::DnsConfig::SecureDnsMode::SECURE;
  else if (doh_mode == chrome_browser_net::kDnsOverHttpsModeAutomatic)
    *secure_dns_mode = net::DnsConfig::SecureDnsMode::AUTOMATIC;
  else
    *secure_dns_mode = net::DnsConfig::SecureDnsMode::OFF;

  std::string doh_templates =
      local_state->GetString(prefs::kDnsOverHttpsTemplates);
  std::string server_method;
  if (!doh_templates.empty() &&
      *secure_dns_mode != net::DnsConfig::SecureDnsMode::OFF) {
    for (const std::string& server_template :
         SplitString(doh_templates, " ", base::TRIM_WHITESPACE,
                     base::SPLIT_WANT_NONEMPTY)) {
      if (!chrome_browser_net::IsValidDohTemplate(server_template,
                                                  &server_method)) {
        continue;
      }

      if (!dns_over_https_servers->has_value()) {
        *dns_over_https_servers = base::make_optional<
            std::vector<network::mojom::DnsOverHttpsServerPtr>>();
      }

      network::mojom::DnsOverHttpsServerPtr dns_over_https_server =
          network::mojom::DnsOverHttpsServer::New();
      dns_over_https_server->server_template = server_template;
      dns_over_https_server->use_post = (server_method == "POST");
      (*dns_over_https_servers)->emplace_back(std::move(dns_over_https_server));
    }
  }
}

void OnStubResolverConfigChanged(PrefService* local_state,
                                 const std::string& pref_name) {
  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
      dns_over_https_servers;
  GetStubResolverConfig(local_state, &insecure_stub_resolver_enabled,
                        &secure_dns_mode, &dns_over_https_servers);
  content::GetNetworkService()->ConfigureStubHostResolver(
      insecure_stub_resolver_enabled, secure_dns_mode,
      std::move(dns_over_https_servers));
}

// Constructs HttpAuthStaticParams based on |local_state|.
network::mojom::HttpAuthStaticParamsPtr CreateHttpAuthStaticParams(
    PrefService* local_state) {
  network::mojom::HttpAuthStaticParamsPtr auth_static_params =
      network::mojom::HttpAuthStaticParams::New();

  // TODO(https://crbug/549273): Allow this to change after startup.
  auth_static_params->supported_schemes =
      base::SplitString(local_state->GetString(prefs::kAuthSchemes), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  auth_static_params->gssapi_library_name =
      local_state->GetString(prefs::kGSSAPILibraryName);
#endif

  return auth_static_params;
}

// Constructs HttpAuthDynamicParams based on |local_state|.
network::mojom::HttpAuthDynamicParamsPtr CreateHttpAuthDynamicParams(
    PrefService* local_state) {
  network::mojom::HttpAuthDynamicParamsPtr auth_dynamic_params =
      network::mojom::HttpAuthDynamicParams::New();

  auth_dynamic_params->server_allowlist =
      local_state->GetString(prefs::kAuthServerWhitelist);
  auth_dynamic_params->delegate_allowlist =
      local_state->GetString(prefs::kAuthNegotiateDelegateWhitelist);
  auth_dynamic_params->negotiate_disable_cname_lookup =
      local_state->GetBoolean(prefs::kDisableAuthNegotiateCnameLookup);
  auth_dynamic_params->enable_negotiate_port =
      local_state->GetBoolean(prefs::kEnableAuthNegotiatePort);

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  auth_dynamic_params->delegate_by_kdc_policy =
      local_state->GetBoolean(prefs::kAuthNegotiateDelegateByKdcPolicy);
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if defined(OS_POSIX)
  auth_dynamic_params->ntlm_v2_enabled =
      local_state->GetBoolean(prefs::kNtlmV2Enabled);
#endif  // defined(OS_POSIX)

#if defined(OS_ANDROID)
  auth_dynamic_params->android_negotiate_account_type =
      local_state->GetString(prefs::kAuthAndroidNegotiateAccountType);
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  auth_dynamic_params->allow_gssapi_library_load =
      connector->IsActiveDirectoryManaged() ||
      local_state->GetBoolean(prefs::kKerberosEnabled);
#endif

  return auth_dynamic_params;
}

void OnAuthPrefsChanged(PrefService* local_state,
                        const std::string& pref_name) {
  content::GetNetworkService()->ConfigureHttpAuthPrefs(
      CreateHttpAuthDynamicParams(local_state));
}

// Check the AsyncDns field trial and return true if it should be enabled. On
// Android this includes checking the Android version in the field trial.
bool ShouldEnableAsyncDns() {
  bool feature_can_be_enabled = true;
#if defined(OS_ANDROID)
  int min_sdk =
      base::GetFieldTrialParamByFeatureAsInt(features::kAsyncDns, "min_sdk", 0);
  if (base::android::BuildInfo::GetInstance()->sdk_int() < min_sdk)
    feature_can_be_enabled = false;
#endif
  return feature_can_be_enabled &&
         base::FeatureList::IsEnabled(features::kAsyncDns);
}

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
bool ShouldUseBuiltinCertVerifier(PrefService* local_state) {
#if BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
  const PrefService::Preference* builtin_cert_verifier_enabled_pref =
      local_state->FindPreference(prefs::kBuiltinCertificateVerifierEnabled);
  if (builtin_cert_verifier_enabled_pref->IsManaged())
    return builtin_cert_verifier_enabled_pref->GetValue()->GetBool();
#endif

  return base::FeatureList::IsEnabled(
      net::features::kCertVerifierBuiltinFeature);
}
#endif

}  // namespace

// SharedURLLoaderFactory backed by a SystemNetworkContextManager and its
// network context. Transparently handles crashes.
class SystemNetworkContextManager::URLLoaderFactoryForSystem
    : public network::SharedURLLoaderFactory {
 public:
  explicit URLLoaderFactoryForSystem(SystemNetworkContextManager* manager)
      : manager_(manager) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  // mojom::URLLoaderFactory implementation:

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
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
        std::move(receiver), routing_id, request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    if (!manager_)
      return;
    manager_->GetURLLoaderFactory()->Clone(std::move(receiver));
  }

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return std::make_unique<network::CrossThreadSharedURLLoaderFactoryInfo>(
        this);
  }

  void Shutdown() { manager_ = nullptr; }

 private:
  friend class base::RefCounted<URLLoaderFactoryForSystem>;
  ~URLLoaderFactoryForSystem() override {}

  SEQUENCE_CHECKER(sequence_checker_);
  SystemNetworkContextManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryForSystem);
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
  params->is_corb_enabled = false;
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

    // TODO(crbug.com/981057): There should be a DCHECK() here to make sure
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

SystemNetworkContextManager::SystemNetworkContextManager(
    PrefService* local_state)
    : local_state_(local_state),
      ssl_config_service_manager_(
          SSLConfigServiceManager::CreateDefaultManager(local_state_)),
      proxy_config_monitor_(local_state_) {
#if !defined(OS_ANDROID)
  // QuicAllowed was not part of Android policy.
  const base::Value* value =
      g_browser_process->policy_service()
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()))
          .GetValue(policy::key::kQuicAllowed);
  if (value)
    value->GetAsBoolean(&is_quic_allowed_);
#endif
  shared_url_loader_factory_ = new URLLoaderFactoryForSystem(this);

  pref_change_registrar_.Init(local_state_);

  // Update the DnsClient and DoH default preferences based on the corresponding
  // features before registering change callbacks for these preferences.
  local_state_->SetDefaultPrefValue(prefs::kBuiltInDnsClientEnabled,
                                    base::Value(ShouldEnableAsyncDns()));
  std::string default_doh_mode = chrome_browser_net::kDnsOverHttpsModeOff;
  std::string default_doh_templates = "";
  if (base::FeatureList::IsEnabled(features::kDnsOverHttps)) {
    if (features::kDnsOverHttpsFallbackParam.Get()) {
      default_doh_mode = chrome_browser_net::kDnsOverHttpsModeAutomatic;
    } else {
      default_doh_mode = chrome_browser_net::kDnsOverHttpsModeSecure;
    }
    default_doh_templates = features::kDnsOverHttpsTemplatesParam.Get();
  }
  local_state_->SetDefaultPrefValue(prefs::kDnsOverHttpsMode,
                                    base::Value(default_doh_mode));
  local_state_->SetDefaultPrefValue(prefs::kDnsOverHttpsTemplates,
                                    base::Value(default_doh_templates));

  // If the user has explicitly enabled or disabled the DoH experiment in
  // chrome://flags, store that choice in the user prefs so that it can be
  // persisted after the experiment ends. Also make sure to remove the stored
  // prefs value if the user has changed their chrome://flags selection to the
  // default.
  flags_ui::PrefServiceFlagsStorage flags_storage(local_state_);
  std::set<std::string> entries = flags_storage.GetFlags();
  if (entries.count("dns-over-https@1")) {
    // The user has "Enabled" selected.
    local_state_->SetString(prefs::kDnsOverHttpsMode,
                            chrome_browser_net::kDnsOverHttpsModeAutomatic);
  } else if (entries.count("dns-over-https@2")) {
    // The user has "Disabled" selected.
    local_state_->SetString(prefs::kDnsOverHttpsMode,
                            chrome_browser_net::kDnsOverHttpsModeOff);
  } else {
    // The user has "Default" selected.
    local_state_->ClearPref(prefs::kDnsOverHttpsMode);
  }

  PrefChangeRegistrar::NamedChangeCallback dns_pref_callback =
      base::BindRepeating(&OnStubResolverConfigChanged,
                          base::Unretained(local_state_));
  pref_change_registrar_.Add(prefs::kBuiltInDnsClientEnabled,
                             dns_pref_callback);
  pref_change_registrar_.Add(prefs::kDnsOverHttpsMode, dns_pref_callback);
  pref_change_registrar_.Add(prefs::kDnsOverHttpsTemplates, dns_pref_callback);

  PrefChangeRegistrar::NamedChangeCallback auth_pref_callback =
      base::BindRepeating(&OnAuthPrefsChanged, base::Unretained(local_state_));
  pref_change_registrar_.Add(prefs::kAuthServerWhitelist, auth_pref_callback);
  pref_change_registrar_.Add(prefs::kAuthNegotiateDelegateWhitelist,
                             auth_pref_callback);
  pref_change_registrar_.Add(prefs::kDisableAuthNegotiateCnameLookup,
                             auth_pref_callback);
  pref_change_registrar_.Add(prefs::kEnableAuthNegotiatePort,
                             auth_pref_callback);

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  pref_change_registrar_.Add(prefs::kAuthNegotiateDelegateByKdcPolicy,
                             auth_pref_callback);
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if defined(OS_POSIX)
  pref_change_registrar_.Add(prefs::kNtlmV2Enabled, auth_pref_callback);
#endif  // defined(OS_POSIX)

#if defined(OS_ANDROID)
  pref_change_registrar_.Add(prefs::kAuthAndroidNegotiateAccountType,
                             auth_pref_callback);
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  pref_change_registrar_.Add(prefs::kKerberosEnabled, auth_pref_callback);
#endif  // defined(OS_CHROMEOS)

  local_state_->SetDefaultPrefValue(
      prefs::kEnableReferrers,
      base::Value(!base::FeatureList::IsEnabled(features::kNoReferrers)));
  enable_referrers_.Init(
      prefs::kEnableReferrers, local_state_,
      base::BindRepeating(&SystemNetworkContextManager::UpdateReferrersEnabled,
                          base::Unretained(this)));
}

SystemNetworkContextManager::~SystemNetworkContextManager() {
  shared_url_loader_factory_->Shutdown();
}

void SystemNetworkContextManager::RegisterPrefs(PrefRegistrySimple* registry) {
  // Register the DnsClient and DoH preferences. The feature list has not been
  // initialized yet, so setting the preference defaults here to reflect the
  // corresponding features will only cause the preference defaults to reflect
  // the feature defaults (feature values set via the command line will not be
  // captured). Thus, the preference defaults are updated in the constructor
  // for SystemNetworkContextManager, at which point the feature list is ready.
  registry->RegisterBooleanPref(prefs::kBuiltInDnsClientEnabled, false);
  registry->RegisterStringPref(prefs::kDnsOverHttpsMode, std::string());
  registry->RegisterStringPref(prefs::kDnsOverHttpsTemplates, std::string());

  // Static auth params
  registry->RegisterStringPref(prefs::kAuthSchemes,
                               "basic,digest,ntlm,negotiate");
#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  registry->RegisterStringPref(prefs::kGSSAPILibraryName, std::string());
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

  // Dynamic auth params.
  registry->RegisterBooleanPref(prefs::kDisableAuthNegotiateCnameLookup, false);
  registry->RegisterBooleanPref(prefs::kEnableAuthNegotiatePort, false);
  registry->RegisterStringPref(prefs::kAuthServerWhitelist, std::string());
  registry->RegisterStringPref(prefs::kAuthNegotiateDelegateWhitelist,
                               std::string());
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kAuthNegotiateDelegateByKdcPolicy,
                                false);
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if defined(OS_POSIX)
  registry->RegisterBooleanPref(
      prefs::kNtlmV2Enabled,
      base::FeatureList::IsEnabled(features::kNtlmV2Enabled));
#endif  // defined(OS_POSIX)
#if defined(OS_ANDROID)
  registry->RegisterStringPref(prefs::kAuthAndroidNegotiateAccountType,
                               std::string());
#endif  // defined(OS_ANDROID)

  // Per-NetworkContext pref. The pref value from |local_state_| is used for
  // the system NetworkContext, and the per-profile pref values are used for
  // the profile NetworkContexts.
  registry->RegisterBooleanPref(prefs::kEnableReferrers, true);

  registry->RegisterBooleanPref(prefs::kQuickCheckEnabled, true);

  registry->RegisterIntegerPref(prefs::kMaxConnectionsPerProxy, -1);

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
  // Note that the default value is not relevant because the pref is only
  // evaluated when it is managed.
  registry->RegisterBooleanPref(prefs::kBuiltinCertificateVerifierEnabled,
                                false);
#endif
}

void SystemNetworkContextManager::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  // Disable QUIC globally, if needed.
  if (!is_quic_allowed_)
    network_service->DisableQuic();

  network_service->SetUpHttpAuth(CreateHttpAuthStaticParams(local_state_));
  network_service->ConfigureHttpAuthPrefs(
      CreateHttpAuthDynamicParams(local_state_));

  // TODO(lukasza): https://crbug.com/944162: Once
  // kMimeHandlerViewInCrossProcessFrame feature ships, unconditionally include
  // the MIME types below in GetNeverSniffedMimeTypes in
  // services/network/cross_origin_read_blocking.cc.  Without
  // kMimeHandlerViewInCrossProcessFrame feature, PDFs and other similar
  // MimeHandlerView-handled resources may be fetched from a cross-origin
  // renderer (see https://crbug.com/929300).
  if (content::MimeHandlerViewMode::UsesCrossProcessFrame()) {
    network_service->AddExtraMimeTypesForCorb(
        {"application/msexcel",
         "application/mspowerpoint",
         "application/msword",
         "application/msword-template",
         "application/pdf",
         "application/vnd.ces-quickpoint",
         "application/vnd.ces-quicksheet",
         "application/vnd.ces-quickword",
         "application/vnd.ms-excel",
         "application/vnd.ms-excel.sheet.macroenabled.12",
         "application/vnd.ms-powerpoint",
         "application/vnd.ms-powerpoint.presentation.macroenabled.12",
         "application/vnd.ms-word",
         "application/vnd.ms-word.document.12",
         "application/vnd.ms-word.document.macroenabled.12",
         "application/vnd.msword",
         "application/"
         "vnd.openxmlformats-officedocument.presentationml.presentation",
         "application/"
         "vnd.openxmlformats-officedocument.presentationml.template",
         "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
         "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
         "application/"
         "vnd.openxmlformats-officedocument.wordprocessingml.document",
         "application/"
         "vnd.openxmlformats-officedocument.wordprocessingml.template",
         "application/vnd.presentation-openxml",
         "application/vnd.presentation-openxmlm",
         "application/vnd.spreadsheet-openxml",
         "application/vnd.wordprocessing-openxml",
         "text/csv"});
  }

  int max_connections_per_proxy =
      local_state_->GetInteger(prefs::kMaxConnectionsPerProxy);
  if (max_connections_per_proxy != -1)
    network_service->SetMaxConnectionsPerProxy(max_connections_per_proxy);

  // The system NetworkContext must be created first, since it sets
  // |primary_network_context| to true.
  network_service_network_context_.reset();
  network_service->CreateNetworkContext(
      network_service_network_context_.BindNewPipeAndPassReceiver(),
      CreateNetworkContextParams());

  mojo::PendingRemote<network::mojom::NetworkContextClient> client_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<content::NetworkContextClientBase>(),
      client_remote.InitWithNewPipeAndPassReceiver());
  network_service_network_context_->SetClient(std::move(client_remote));

  // Configure the stub resolver. This must be done after the system
  // NetworkContext is created, but before anything has the chance to use it.
  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
      dns_over_https_servers;
  GetStubResolverConfig(local_state_, &insecure_stub_resolver_enabled,
                        &secure_dns_mode, &dns_over_https_servers);
  content::GetNetworkService()->ConfigureStubHostResolver(
      insecure_stub_resolver_enabled, secure_dns_mode,
      std::move(dns_over_https_servers));

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Set up crypt config. This should be kept in sync with the OSCrypt parts of
  // ChromeBrowserMainPartsLinux::PreProfileInit.
  network::mojom::CryptConfigPtr config = network::mojom::CryptConfig::New();
  config->store = command_line.GetSwitchValueASCII(switches::kPasswordStore);
  config->product_name = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  config->should_use_preference =
      command_line.HasSwitch(switches::kEnableEncryptionSelection);
  chrome::GetDefaultUserDataDirectory(&config->user_data_path);
  content::GetNetworkService()->SetCryptConfig(std::move(config));
#endif
#if defined(OS_WIN) || defined(OS_MACOSX)
  // The OSCrypt keys are process bound, so if network service is out of
  // process, send it the required key.
  if (content::IsOutOfProcessNetworkService()) {
    content::GetNetworkService()->SetEncryptionKey(
        OSCrypt::GetRawEncryptionKey());
  }
#endif

  // Asynchronously reapply the most recently received CRLSet (if any).
  component_updater::CRLSetPolicy::ReconfigureAfterNetworkRestart();
}

void SystemNetworkContextManager::DisableQuic() {
  is_quic_allowed_ = false;

  // Disabling QUIC for a profile disables QUIC globally. As a side effect, new
  // Profiles will also have QUIC disabled because the network service will
  // disable QUIC.
  content::GetNetworkService()->DisableQuic();
}

void SystemNetworkContextManager::AddSSLConfigToNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params) {
  ssl_config_service_manager_->AddToNetworkContextParams(
      network_context_params);
}

network::mojom::NetworkContextParamsPtr
SystemNetworkContextManager::CreateDefaultNetworkContextParams() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  content::UpdateCorsExemptHeader(network_context_params.get());
  variations::UpdateCorsExemptHeaderForVariations(network_context_params.get());

  network_context_params->enable_brotli = true;

  network_context_params->user_agent = GetUserAgent();

  // Disable referrers by default. Any consumer that enables referrers should
  // respect prefs::kEnableReferrers from the appropriate pref store.
  network_context_params->enable_referrers = false;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  std::string quic_user_agent_id;

  if (base::FeatureList::IsEnabled(blink::features::kFreezeUserAgent)) {
    quic_user_agent_id = "";
  } else {
    quic_user_agent_id = chrome::GetChannelName();
    if (!quic_user_agent_id.empty())
      quic_user_agent_id.push_back(' ');
    quic_user_agent_id.append(
        version_info::GetProductNameAndVersionForUserAgent());
    quic_user_agent_id.push_back(' ');
    quic_user_agent_id.append(
        content::BuildOSCpuInfo(false /* include_android_build_number */));
  }
  network_context_params->quic_user_agent_id = quic_user_agent_id;

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
#if defined(OS_CHROMEOS)
      network_context_params->dhcp_wpad_url_client =
          chromeos::DhcpWpadUrlClient::CreateWithSelfOwnedReceiver();
#endif  // defined(OS_CHROMEOS)
    }
  }

  network_context_params->pac_quick_check_enabled =
      local_state_->GetBoolean(prefs::kQuickCheckEnabled);

  // Use the SystemNetworkContextManager to populate and update SSL
  // configuration. The SystemNetworkContextManager is owned by the
  // BrowserProcess itself, so will only be destroyed on shutdown, at which
  // point, all NetworkContexts will be destroyed as well.
  AddSSLConfigToNetworkContextParams(network_context_params.get());

#if !defined(OS_ANDROID)

  if (g_enable_certificate_transparency) {
    network_context_params->enforce_chrome_ct_policy = true;
    network_context_params->ct_log_update_time = base::GetBuildTime();

    std::vector<std::string> operated_by_google_logs =
        certificate_transparency::GetLogsOperatedByGoogle();
    std::vector<std::pair<std::string, base::TimeDelta>> disqualified_logs =
        certificate_transparency::GetDisqualifiedLogs();
    for (const auto& ct_log : certificate_transparency::GetKnownLogs()) {
      // TODO(rsleevi): https://crbug.com/702062 - Remove this duplication.
      network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
      log_info->public_key = std::string(ct_log.log_key, ct_log.log_key_length);
      log_info->name = ct_log.log_name;

      std::string log_id = crypto::SHA256HashString(log_info->public_key);
      log_info->operated_by_google =
          std::binary_search(std::begin(operated_by_google_logs),
                             std::end(operated_by_google_logs), log_id);
      auto it = std::lower_bound(
          std::begin(disqualified_logs), std::end(disqualified_logs), log_id,
          [](const auto& disqualified_log, const std::string& log_id) {
            return disqualified_log.first < log_id;
          });
      if (it != std::end(disqualified_logs) && it->first == log_id) {
        log_info->disqualified_at = it->second;
      }
      network_context_params->ct_logs.push_back(std::move(log_info));
    }
  }
#endif

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
  network_context_params->use_builtin_cert_verifier =
      ShouldUseBuiltinCertVerifier(local_state_);
#endif

  return network_context_params;
}

net_log::NetExportFileWriter*
SystemNetworkContextManager::GetNetExportFileWriter() {
  if (!net_export_file_writer_) {
    net_export_file_writer_ = std::make_unique<net_log::NetExportFileWriter>();
  }
  return net_export_file_writer_.get();
}

void SystemNetworkContextManager::FlushSSLConfigManagerForTesting() {
  ssl_config_service_manager_->FlushForTesting();
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

void SystemNetworkContextManager::GetStubResolverConfigForTesting(
    bool* insecure_stub_resolver_enabled,
    net::DnsConfig::SecureDnsMode* secure_dns_mode,
    base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>*
        dns_over_https_servers) {
  GetStubResolverConfig(g_browser_process->local_state(),
                        insecure_stub_resolver_enabled, secure_dns_mode,
                        dns_over_https_servers);
}

network::mojom::HttpAuthStaticParamsPtr
SystemNetworkContextManager::GetHttpAuthStaticParamsForTesting() {
  return CreateHttpAuthStaticParams(g_browser_process->local_state());
}

network::mojom::HttpAuthDynamicParamsPtr
SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting() {
  return CreateHttpAuthDynamicParams(g_browser_process->local_state());
}

void SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
    base::Optional<bool> enabled) {
  g_enable_certificate_transparency =
      enabled.value_or(kCertificateTransparencyEnabled);
}

network::mojom::NetworkContextParamsPtr
SystemNetworkContextManager::CreateNetworkContextParams() {
  // TODO(mmenke): Set up parameters here (in memory cookie store, etc).
  network::mojom::NetworkContextParamsPtr network_context_params =
      CreateDefaultNetworkContextParams();

  network_context_params->context_name = std::string("system");

  network_context_params->enable_referrers = enable_referrers_.GetValue();

  network_context_params->http_cache_enabled = false;

  // These are needed for PAC scripts that use FTP URLs.
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  network_context_params->enable_ftp_url_support =
      base::FeatureList::IsEnabled(features::kFtpProtocol);
#endif

  network_context_params->primary_network_context = true;

  proxy_config_monitor_.AddToNetworkContextParams(network_context_params.get());

  return network_context_params;
}

void SystemNetworkContextManager::UpdateReferrersEnabled() {
  GetContext()->SetEnableReferrers(enable_referrers_.GetValue());
}
