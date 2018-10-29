// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_io_data.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/chrome_url_request_context_getter.h"
#include "chrome/browser/net/failing_url_request_interceptor.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/policy/cloud/policy_header_service_factory.h"
#include "chrome/browser/policy/policy_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/about_handler/about_protocol_handler.h"
#include "components/certificate_transparency/tree_state_tracker.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/domain_reliability/monitor.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/net_log/chrome_net_log.h"
#include "components/policy/core/common/cloud/policy_header_service.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/sync/base/pref_names.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_network_transaction_factory.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/layered_network_delegate.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_persister.h"
#include "net/net_buildflags.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/client_cert_store.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/ftp_protocol_handler.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "services/network/ignore_errors_cert_verifier.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/proxy_config_mojom_traits.h"
#include "services/network/url_request_context_builder_mojo.h"
#include "third_party/blink/public/public_buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if defined(OS_ANDROID)
#include "content/public/browser/android/content_protocol_handler.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/fileapi/external_file_protocol_handler.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/net/client_cert_filter_chromeos.h"
#include "chrome/browser/chromeos/net/client_cert_store_chromeos.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/net/nss_context.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/tpm_token_info_getter.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "services/network/cert_verifier_with_trust_anchors.h"
#include "services/network/cert_verify_proc_chromeos.h"
#endif  // defined(OS_CHROMEOS)

#if defined(USE_NSS_CERTS)
#include "chrome/browser/ui/crypto_module_delegate_nss.h"
#include "net/ssl/client_cert_store_nss.h"
#endif  // defined(USE_NSS_CERTS)

#if defined(OS_WIN)
#include "net/ssl/client_cert_store_win.h"
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
#include "net/ssl/client_cert_store_mac.h"
#endif  // defined(OS_MACOSX)

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_MACOSX)
#include "chrome/browser/net/trial_comparison_cert_verifier.h"
#include "net/cert/cert_verify_proc_builtin.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::ResourceContext;

namespace {

net::CertVerifier* g_cert_verifier_for_profile_io_data_testing = nullptr;

// A CertVerifier that forwards all requests to
// |g_cert_verifier_for_profile_io_data_testing|. This is used to allow Profiles
// to have their own std::unique_ptr<net::CertVerifier> while forwarding calls
// to the shared verifier.
class WrappedCertVerifierForProfileIODataTesting : public net::CertVerifier {
 public:
  ~WrappedCertVerifierForProfileIODataTesting() override = default;

  // CertVerifier implementation
  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    verify_result->Reset();
    if (!g_cert_verifier_for_profile_io_data_testing)
      return net::ERR_FAILED;
    return g_cert_verifier_for_profile_io_data_testing->Verify(
        params, verify_result, std::move(callback), out_req, net_log);
  }
  void SetConfig(const Config& config) override {
    if (!g_cert_verifier_for_profile_io_data_testing)
      return;
    return g_cert_verifier_for_profile_io_data_testing->SetConfig(config);
  }
};

#if defined(OS_CHROMEOS)
// The following four functions are responsible for initializing NSS for each
// profile on ChromeOS, which has a separate NSS database and TPM slot
// per-profile.
//
// Initialization basically follows these steps:
// 1) Get some info from user_manager::UserManager about the User for this
// profile.
// 2) Tell nss_util to initialize the software slot for this profile.
// 3) Wait for the TPM module to be loaded by nss_util if it isn't already.
// 4) Ask CryptohomeClient which TPM slot id corresponds to this profile.
// 5) Tell nss_util to use that slot id on the TPM module.
//
// Some of these steps must happen on the UI thread, others must happen on the
// IO thread:
//               UI thread                              IO Thread
//
//  ProfileIOData::InitializeOnUIThread
//                   |
//  ProfileHelper::Get()->GetUserByProfile()
//                   \---------------------------------------v
//                                                 StartNSSInitOnIOThread
//                                                           |
//                                          crypto::InitializeNSSForChromeOSUser
//                                                           |
//                                                crypto::IsTPMTokenReady
//                                                           |
//                                          StartTPMSlotInitializationOnIOThread
//                   v---------------------------------------/
//     GetTPMInfoForUserOnUIThread
//                   |
// chromeos::TPMTokenInfoGetter::Start
//                   |
//     DidGetTPMInfoForUserOnUIThread
//                   \---------------------------------------v
//                                          crypto::InitializeTPMForChromeOSUser

void DidGetTPMInfoForUserOnUIThread(
    std::unique_ptr<chromeos::TPMTokenInfoGetter> getter,
    const std::string& username_hash,
    base::Optional<chromeos::CryptohomeClient::TpmTokenInfo> token_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (token_info.has_value() && token_info->slot != -1) {
    DVLOG(1) << "Got TPM slot for " << username_hash << ": "
             << token_info->slot;
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                             base::Bind(&crypto::InitializeTPMForChromeOSUser,
                                        username_hash, token_info->slot));
  } else {
    NOTREACHED() << "TPMTokenInfoGetter reported invalid token.";
  }
}

void GetTPMInfoForUserOnUIThread(const AccountId& account_id,
                                 const std::string& username_hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(1) << "Getting TPM info from cryptohome for "
           << " " << account_id.Serialize() << " " << username_hash;
  std::unique_ptr<chromeos::TPMTokenInfoGetter> scoped_token_info_getter =
      chromeos::TPMTokenInfoGetter::CreateForUserToken(
          account_id, chromeos::DBusThreadManager::Get()->GetCryptohomeClient(),
          base::ThreadTaskRunnerHandle::Get());
  chromeos::TPMTokenInfoGetter* token_info_getter =
      scoped_token_info_getter.get();

  // Bind |token_info_getter| to the callback to ensure it does not go away
  // before TPM token info is fetched.
  // TODO(tbarzic, pneubeck): Handle this in a nicer way when this logic is
  //     moved to a separate profile service.
  token_info_getter->Start(base::BindOnce(&DidGetTPMInfoForUserOnUIThread,
                                          std::move(scoped_token_info_getter),
                                          username_hash));
}

void StartTPMSlotInitializationOnIOThread(const AccountId& account_id,
                                          const std::string& username_hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::Bind(&GetTPMInfoForUserOnUIThread, account_id, username_hash));
}

void StartNSSInitOnIOThread(const AccountId& account_id,
                            const std::string& username_hash,
                            const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "Starting NSS init for " << account_id.Serialize()
           << "  hash:" << username_hash;

  // Make sure NSS is initialized for the user.
  crypto::InitializeNSSForChromeOSUser(username_hash, path);

  // Check if it's OK to initialize TPM for the user before continuing. This
  // may not be the case if the TPM slot initialization was previously
  // requested for the same user.
  if (!crypto::ShouldInitializeTPMForChromeOSUser(username_hash))
    return;

  crypto::WillInitializeTPMForChromeOSUser(username_hash);

  if (crypto::IsTPMTokenEnabledForNSS()) {
    if (crypto::IsTPMTokenReady(
            base::Bind(&StartTPMSlotInitializationOnIOThread, account_id,
                       username_hash))) {
      StartTPMSlotInitializationOnIOThread(account_id, username_hash);
    } else {
      DVLOG(1) << "Waiting for tpm ready ...";
    }
  } else {
    crypto::InitializePrivateSoftwareSlotForChromeOSUser(username_hash);
  }
}
#endif  // defined(OS_CHROMEOS)

// For safe shutdown, must be called before the ProfileIOData is destroyed.
void NotifyContextGettersOfShutdownOnIO(
    std::unique_ptr<ProfileIOData::ChromeURLRequestContextGetterVector>
        getters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto& chrome_context_getter : *getters)
    chrome_context_getter->NotifyContextShuttingDown();
}

// Wraps |inner_job_factory| with |protocol_handler_interceptor|.
std::unique_ptr<net::URLRequestJobFactory> CreateURLRequestJobFactory(
    std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor,
    std::unique_ptr<net::URLRequestJobFactory> inner_job_factory) {
  protocol_handler_interceptor->Chain(std::move(inner_job_factory));
  return std::move(protocol_handler_interceptor);
}

}  // namespace

void ProfileIOData::InitializeOnUIThread(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrefService* pref_service = profile->GetPrefs();

  std::unique_ptr<ProfileParams> params(new ProfileParams);
  params->path = profile->GetPath();

  params->io_thread = g_browser_process->io_thread();

  ProfileNetworkContextServiceFactory::GetForContext(profile)
      ->SetUpProfileIODataNetworkContext(
          profile->IsOffTheRecord() /* in_memory */,
          base::FilePath() /* relative_partition_path */,
          &params->main_network_context_request,
          &params->main_network_context_params);

  params->cookie_settings = CookieSettingsFactory::GetForProfile(profile);
  params->host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  params->extension_info_map =
      extensions::ExtensionSystem::Get(profile)->info_map();
#endif

  params->account_consistency =
      AccountConsistencyModeManager::GetMethodForProfile(profile);

  ProtocolHandlerRegistry* protocol_handler_registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);
  DCHECK(protocol_handler_registry);

  // The profile instance is only available here in the InitializeOnUIThread
  // method, so we create the url job factory here, then save it for
  // later delivery to the job factory in Init().
  params->protocol_handler_interceptor =
      protocol_handler_registry->CreateJobInterceptorFactory();

#if defined(OS_CHROMEOS)
  // Enable client certificates for the Chrome OS sign-in frame, if this feature
  // is not disabled by a flag.
  // Note that while this applies to the whole sign-in profile, client
  // certificates will only be selected for the StoragePartition currently used
  // in the sign-in frame (see SigninPartitionManager).
  if (chromeos::switches::IsSigninFrameClientCertsEnabled() &&
      chromeos::ProfileHelper::IsSigninProfile(profile)) {
    // We only need the system slot for client certificates, not in NSS context
    // (the sign-in profile's NSS context is not initialized).
    params->system_key_slot_use_type = SystemKeySlotUseType::kUseForClientAuth;
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager) {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
    // No need to initialize NSS for users with empty username hash:
    // Getters for a user's NSS slots always return NULL slot if the user's
    // username hash is empty, even when the NSS is not initialized for the
    // user.
    if (user && !user->username_hash().empty()) {
      params->username_hash = user->username_hash();
      DCHECK(!params->username_hash.empty());
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::Bind(&StartNSSInitOnIOThread, user->GetAccountId(),
                     user->username_hash(), profile->GetPath()));

      // Use the device-wide system key slot only if the user is affiliated on
      // the device.
      if (user->IsAffiliated()) {
        params->system_key_slot_use_type =
            SystemKeySlotUseType::kUseForClientAuthAndCertManagement;
      }
    }
  }

  chromeos::CertificateProviderService* cert_provider_service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          profile);
  if (cert_provider_service) {
    params->certificate_provider =
        cert_provider_service->CreateCertificateProvider();
  }
#endif

  params->profile = profile;
  profile_params_ = std::move(params);

  force_google_safesearch_.Init(prefs::kForceGoogleSafeSearch, pref_service);
  force_google_safesearch_.MoveToThread(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
  force_youtube_restrict_.Init(prefs::kForceYouTubeRestrict, pref_service);
  force_youtube_restrict_.MoveToThread(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
  allowed_domains_for_apps_.Init(prefs::kAllowedDomainsForApps, pref_service);
  allowed_domains_for_apps_.MoveToThread(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});

  // These members are used only for sign in, which is not enabled
  // in incognito mode.  So no need to initialize them.
  if (!IsOffTheRecord()) {
    google_services_user_account_id_.Init(
        prefs::kGoogleServicesUserAccountId, pref_service);
    google_services_user_account_id_.MoveToThread(io_task_runner);
    sync_suppress_start_.Init(syncer::prefs::kSyncSuppressStart, pref_service);
    sync_suppress_start_.MoveToThread(io_task_runner);
    sync_first_setup_complete_.Init(syncer::prefs::kSyncFirstSetupComplete,
                                    pref_service);
    sync_first_setup_complete_.MoveToThread(io_task_runner);
    sync_has_auth_error_.Init(syncer::prefs::kSyncHasAuthError, pref_service);
    sync_has_auth_error_.MoveToThread(io_task_runner);
  }

#if !defined(OS_CHROMEOS)
  signin_scoped_device_id_.Init(prefs::kGoogleServicesSigninScopedDeviceId,
                                pref_service);
  signin_scoped_device_id_.MoveToThread(io_task_runner);
#endif

  network_prediction_options_.Init(prefs::kNetworkPredictionOptions,
                                   pref_service);

  network_prediction_options_.MoveToThread(io_task_runner);

#if defined(OS_CHROMEOS)
  if (!g_cert_verifier_for_profile_io_data_testing &&
      !base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    profile_params_->policy_cert_verifier =
        policy::PolicyCertServiceFactory::CreateForProfile(profile);
  }
#endif

  incognito_availibility_pref_.Init(
      prefs::kIncognitoModeAvailability, pref_service);
  incognito_availibility_pref_.MoveToThread(io_task_runner);

#if defined(OS_CHROMEOS)
  account_consistency_mirror_required_pref_.Init(
      prefs::kAccountConsistencyMirrorRequired, pref_service);
  account_consistency_mirror_required_pref_.MoveToThread(io_task_runner);
#endif

  // We need to make sure that content initializes its own data structures that
  // are associated with each ResourceContext because we might post this
  // object to the IO thread after this function.
  BrowserContext::EnsureResourceContextInitialized(profile);
}

ProfileIOData::MediaRequestContext::MediaRequestContext(const char* name) {
  set_name(name);
}

void ProfileIOData::MediaRequestContext::SetHttpTransactionFactory(
    std::unique_ptr<net::HttpTransactionFactory> http_factory) {
  http_factory_ = std::move(http_factory);
  set_http_transaction_factory(http_factory_.get());
}

ProfileIOData::MediaRequestContext::~MediaRequestContext() {
  AssertNoURLRequests();
}

ProfileIOData::AppRequestContext::AppRequestContext() {
  set_name("app_request");
}

void ProfileIOData::AppRequestContext::SetCookieStore(
    std::unique_ptr<net::CookieStore> cookie_store) {
  cookie_store_ = std::move(cookie_store);
  set_cookie_store(cookie_store_.get());
}

void ProfileIOData::AppRequestContext::SetChannelIDService(
    std::unique_ptr<net::ChannelIDService> channel_id_service) {
  channel_id_service_ = std::move(channel_id_service);
  set_channel_id_service(channel_id_service_.get());
}

void ProfileIOData::AppRequestContext::SetHttpNetworkSession(
    std::unique_ptr<net::HttpNetworkSession> http_network_session) {
  http_network_session_ = std::move(http_network_session);
}

void ProfileIOData::AppRequestContext::SetHttpTransactionFactory(
    std::unique_ptr<net::HttpTransactionFactory> http_factory) {
  http_factory_ = std::move(http_factory);
  set_http_transaction_factory(http_factory_.get());
}

void ProfileIOData::AppRequestContext::SetJobFactory(
    std::unique_ptr<net::URLRequestJobFactory> job_factory) {
  job_factory_ = std::move(job_factory);
  set_job_factory(job_factory_.get());
}

#if BUILDFLAG(ENABLE_REPORTING)
void ProfileIOData::AppRequestContext::SetReportingService(
    std::unique_ptr<net::ReportingService> reporting_service) {
  reporting_service_ = std::move(reporting_service);
  set_reporting_service(reporting_service_.get());
}

void ProfileIOData::AppRequestContext::SetNetworkErrorLoggingService(
    std::unique_ptr<net::NetworkErrorLoggingService>
        network_error_logging_service) {
  network_error_logging_service_ = std::move(network_error_logging_service);
  set_network_error_logging_service(network_error_logging_service_.get());
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

ProfileIOData::AppRequestContext::~AppRequestContext() {
#if BUILDFLAG(ENABLE_REPORTING)
  SetNetworkErrorLoggingService(nullptr);
  SetReportingService(nullptr);
#endif  // BUILDFLAG(ENABLE_REPORTING)
  AssertNoURLRequests();
}

ProfileIOData::ProfileParams::ProfileParams() = default;

ProfileIOData::ProfileParams::~ProfileParams() = default;

ProfileIOData::ProfileIOData(Profile::ProfileType profile_type)
    : initialized_(false),
      account_consistency_(signin::AccountConsistencyMethod::kDisabled),
#if defined(OS_CHROMEOS)
      system_key_slot_use_type_(SystemKeySlotUseType::kNone),
#endif
      main_request_context_(nullptr),
      resource_context_(new ResourceContext(this)),
      chrome_network_delegate_unowned_(nullptr),
      domain_reliability_monitor_unowned_(nullptr),
      profile_type_(profile_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ProfileIOData::~ProfileIOData() {
  if (BrowserThread::IsThreadInitialized(BrowserThread::IO))
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Pull the contents of the request context maps onto the stack for sanity
  // checking of values in a minidump. http://crbug.com/260425
  size_t num_app_contexts = app_request_context_map_.size();
  size_t num_media_contexts = isolated_media_request_context_map_.size();
  size_t current_context = 0;
  static const size_t kMaxCachedContexts = 20;
  net::URLRequestContext* app_context_cache[kMaxCachedContexts] = {0};
  void* app_context_vtable_cache[kMaxCachedContexts] = {0};
  net::URLRequestContext* media_context_cache[kMaxCachedContexts] = {0};
  void* media_context_vtable_cache[kMaxCachedContexts] = {0};
  void* tmp_vtable = NULL;
  base::debug::Alias(&num_app_contexts);
  base::debug::Alias(&num_media_contexts);
  base::debug::Alias(&current_context);
  base::debug::Alias(app_context_cache);
  base::debug::Alias(app_context_vtable_cache);
  base::debug::Alias(media_context_cache);
  base::debug::Alias(media_context_vtable_cache);
  base::debug::Alias(&tmp_vtable);

  current_context = 0;
  for (URLRequestContextMap::const_iterator it =
           app_request_context_map_.begin();
       current_context < kMaxCachedContexts &&
           it != app_request_context_map_.end();
       ++it, ++current_context) {
    app_context_cache[current_context] = it->second;
    memcpy(&app_context_vtable_cache[current_context],
           static_cast<void*>(it->second), sizeof(void*));
  }

  current_context = 0;
  for (URLRequestContextMap::const_iterator it =
           isolated_media_request_context_map_.begin();
       current_context < kMaxCachedContexts &&
           it != isolated_media_request_context_map_.end();
       ++it, ++current_context) {
    media_context_cache[current_context] = it->second;
    memcpy(&media_context_vtable_cache[current_context],
           static_cast<void*>(it->second), sizeof(void*));
  }

  if (domain_reliability_monitor_unowned_)
    domain_reliability_monitor_unowned_->Shutdown();

  current_context = 0;
  for (auto it = isolated_media_request_context_map_.begin();
       it != isolated_media_request_context_map_.end(); ++it) {
    if (current_context < kMaxCachedContexts) {
      CHECK_EQ(media_context_cache[current_context], it->second);
      memcpy(&tmp_vtable, static_cast<void*>(it->second), sizeof(void*));
      CHECK_EQ(media_context_vtable_cache[current_context], tmp_vtable);
    }
    it->second->AssertNoURLRequests();
    delete it->second;
    current_context++;
  }
}

// static
ProfileIOData* ProfileIOData::FromResourceContext(
    content::ResourceContext* rc) {
  return (static_cast<ResourceContext*>(rc))->io_data_;
}

// static
bool ProfileIOData::IsHandledProtocol(const std::string& scheme) {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  static const char* const kProtocolList[] = {
    url::kFileScheme,
    content::kChromeDevToolsScheme,
    dom_distiller::kDomDistillerScheme,
#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions::kExtensionScheme,
#endif
    content::kChromeUIScheme,
    url::kDataScheme,
#if defined(OS_CHROMEOS)
    content::kExternalFileScheme,
#endif  // defined(OS_CHROMEOS)
#if defined(OS_ANDROID)
    url::kContentScheme,
#endif  // defined(OS_ANDROID)
    url::kAboutScheme,
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
    url::kFtpScheme,
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)
    url::kBlobScheme,
    url::kFileSystemScheme,
    chrome::kChromeSearchScheme,
  };
  for (size_t i = 0; i < arraysize(kProtocolList); ++i) {
    if (scheme == kProtocolList[i])
      return true;
  }
  return net::URLRequest::IsHandledProtocol(scheme);
}

// static
bool ProfileIOData::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  return IsHandledProtocol(url.scheme());
}

// static
void ProfileIOData::InstallProtocolHandlers(
    net::URLRequestJobFactoryImpl* job_factory,
    content::ProtocolHandlerMap* protocol_handlers) {
  for (auto it = protocol_handlers->begin(); it != protocol_handlers->end();
       ++it) {
    bool set_protocol =
        job_factory->SetProtocolHandler(it->first, std::move(it->second));
    DCHECK(set_protocol);
  }
  protocol_handlers->clear();
}

// static
void ProfileIOData::AddProtocolHandlersToBuilder(
    net::URLRequestContextBuilder* builder,
    content::ProtocolHandlerMap* protocol_handlers) {
  for (auto& protocol_handler : *protocol_handlers) {
    builder->SetProtocolHandler(protocol_handler.first,
                                std::move(protocol_handler.second));
  }
  protocol_handlers->clear();
}

// static
void ProfileIOData::SetCertVerifierForTesting(
    net::CertVerifier* cert_verifier) {
  g_cert_verifier_for_profile_io_data_testing = cert_verifier;
}

content::ResourceContext* ProfileIOData::GetResourceContext() const {
  return resource_context_.get();
}

net::URLRequestContext* ProfileIOData::GetMainRequestContext() const {
  DCHECK(initialized_);
  return main_request_context_;
}

net::URLRequestContext* ProfileIOData::GetMediaRequestContext() const {
  DCHECK(initialized_);
  net::URLRequestContext* context = AcquireMediaRequestContext();
  DCHECK(context);
  return context;
}

net::URLRequestContext* ProfileIOData::GetIsolatedAppRequestContext(
    IOThread* io_thread,
    net::URLRequestContext* main_context,
    const StoragePartitionDescriptor& partition_descriptor,
    std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors,
    network::mojom::NetworkContextRequest network_context_request,
    network::mojom::NetworkContextParamsPtr network_context_params) const {
  DCHECK(initialized_);
  if (base::ContainsKey(app_request_context_map_, partition_descriptor))
    return app_request_context_map_[partition_descriptor];

  if (!partition_descriptor.in_memory && !IsOffTheRecord()) {
    MaybeDeleteMediaCache(
        partition_descriptor.path.Append(chrome::kMediaCacheDirname));
  }

  // If the network service is enabled, just re-use the same dummy
  // URLRequestContext as for other requests.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return main_request_context_;

  std::unique_ptr<network::URLRequestContextBuilderMojo> builder =
      std::make_unique<network::URLRequestContextBuilderMojo>();
  io_thread->SetUpProxyService(builder.get());
  builder->SetSharedCertVerifier(main_request_context_->cert_verifier());
  if (data_reduction_proxy_io_data_.get()) {
    builder->set_shared_proxy_delegate(
        data_reduction_proxy_io_data_->proxy_delegate());
  }

  AddProtocolHandlersToBuilder(builder.get(), protocol_handlers);

  if (!IsOffTheRecord()) {
    // The data reduction proxy interceptor should be as close to the network
    // as possible.
    request_interceptors.insert(
        request_interceptors.begin(),
        data_reduction_proxy_io_data()->CreateInterceptor());
  }

  SetUpJobFactoryDefaultsForBuilder(builder.get(),
                                    std::move(request_interceptors),
                                    std::move(protocol_handler_interceptor));

  builder->SetCreateHttpTransactionFactoryCallback(
      base::BindOnce(&content::CreateDevToolsNetworkTransactionFactory));
  builder->set_network_delegate(
      net::LayeredNetworkDelegate::CreatePassThroughNetworkDelegate(
          chrome_network_delegate_unowned_));

  net::URLRequestContext* context;
  app_network_contexts_.emplace_back(
      content::GetNetworkServiceImpl()->CreateNetworkContextWithBuilder(
          std::move(network_context_request), std::move(network_context_params),
          std::move(builder), &context));

  app_request_context_map_[partition_descriptor] = context;
  return context;
}

net::URLRequestContext* ProfileIOData::GetIsolatedMediaRequestContext(
    net::URLRequestContext* app_context,
    const StoragePartitionDescriptor& partition_descriptor) const {
  DCHECK(initialized_);
  net::URLRequestContext* context = NULL;
  if (base::ContainsKey(isolated_media_request_context_map_,
                        partition_descriptor)) {
    context = isolated_media_request_context_map_[partition_descriptor];
  } else {
    context = AcquireIsolatedMediaRequestContext(app_context,
                                                 partition_descriptor);
    isolated_media_request_context_map_[partition_descriptor] = context;
  }
  DCHECK(context);
  return context;
}

extensions::InfoMap* ProfileIOData::GetExtensionInfoMap() const {
  DCHECK(initialized_) << "ExtensionSystem not initialized";
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extension_info_map_.get();
#else
  return nullptr;
#endif
}

content_settings::CookieSettings* ProfileIOData::GetCookieSettings() const {
  // Allow either Init() or SetCookieSettingsForTesting() to initialize.
  DCHECK(initialized_ || cookie_settings_.get());
  return cookie_settings_.get();
}

HostContentSettingsMap* ProfileIOData::GetHostContentSettingsMap() const {
  DCHECK(initialized_);
  return host_content_settings_map_.get();
}

bool ProfileIOData::IsSyncEnabled() const {
  return sync_first_setup_complete_.GetValue() &&
         !sync_suppress_start_.GetValue();
}

bool ProfileIOData::SyncHasAuthError() const {
  return sync_has_auth_error_.GetValue();
}

#if !defined(OS_CHROMEOS)
std::string ProfileIOData::GetSigninScopedDeviceId() const {
  return signin_scoped_device_id_.GetValue();
}
#endif

bool ProfileIOData::IsOffTheRecord() const {
  return profile_type() == Profile::INCOGNITO_PROFILE
      || profile_type() == Profile::GUEST_PROFILE;
}

void ProfileIOData::InitializeMetricsEnabledStateOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Prep the PrefMember and send it to the IO thread, since this value will be
  // read from there.
  enable_metrics_.Init(metrics::prefs::kMetricsReportingEnabled,
                       g_browser_process->local_state());
  enable_metrics_.MoveToThread(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
}

bool ProfileIOData::GetMetricsEnabledStateOnIOThread() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return enable_metrics_.GetValue();
}

chrome_browser_net::Predictor* ProfileIOData::GetPredictor() {
  return nullptr;
}

std::unique_ptr<net::ClientCertStore> ProfileIOData::CreateClientCertStore() {
  if (!client_cert_store_factory_.is_null())
    return client_cert_store_factory_.Run();
#if defined(OS_CHROMEOS)
  bool use_system_key_slot =
      system_key_slot_use_type_ == SystemKeySlotUseType::kUseForClientAuth ||
      system_key_slot_use_type_ ==
          SystemKeySlotUseType::kUseForClientAuthAndCertManagement;
  return std::unique_ptr<net::ClientCertStore>(
      new chromeos::ClientCertStoreChromeOS(
          certificate_provider_ ? certificate_provider_->Copy() : nullptr,
          std::make_unique<chromeos::ClientCertFilterChromeOS>(
              use_system_key_slot, username_hash_),
          base::Bind(&CreateCryptoModuleBlockingPasswordDelegate,
                     kCryptoModulePasswordClientAuth)));
#elif defined(USE_NSS_CERTS)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreNSS(
      base::Bind(&CreateCryptoModuleBlockingPasswordDelegate,
                 kCryptoModulePasswordClientAuth)));
#elif defined(OS_WIN)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreWin());
#elif defined(OS_MACOSX)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreMac());
#elif defined(OS_ANDROID)
  // Android does not use the ClientCertStore infrastructure. On Android client
  // cert matching is done by the OS as part of the call to show the cert
  // selection dialog.
  return nullptr;
#else
#error Unknown platform.
#endif
}

void ProfileIOData::set_data_reduction_proxy_io_data(
    std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
        data_reduction_proxy_io_data) const {
  data_reduction_proxy_io_data_ = std::move(data_reduction_proxy_io_data);
}

ProfileIOData::ResourceContext::ResourceContext(ProfileIOData* io_data)
    : io_data_(io_data),
      request_context_(NULL) {
  DCHECK(io_data);
}

ProfileIOData::ResourceContext::~ResourceContext() {}

net::URLRequestContext* ProfileIOData::ResourceContext::GetRequestContext()  {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(io_data_->initialized_);
  return request_context_;
}

void ProfileIOData::Init(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  // The basic logic is implemented here. The specific initialization
  // is done in InitializeInternal(), implemented by subtypes. Static helper
  // functions have been provided to assist in common operations.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!initialized_);
  DCHECK(profile_params_.get());

  IOThread* const io_thread = profile_params_->io_thread;
  IOThread::Globals* const io_thread_globals = io_thread->globals();

  account_consistency_ = profile_params_->account_consistency;

  // Take ownership over these parameters.
  cookie_settings_ = profile_params_->cookie_settings;
  host_content_settings_map_ = profile_params_->host_content_settings_map;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_info_map_ = profile_params_->extension_info_map;
#endif

#if defined(OS_CHROMEOS)
  username_hash_ = profile_params_->username_hash;
  system_key_slot_use_type_ = profile_params_->system_key_slot_use_type;
  // If we're using the system slot for certificate management, we also must
  // have access to the user's slots.
  DCHECK(!(username_hash_.empty() &&
           system_key_slot_use_type_ ==
               SystemKeySlotUseType::kUseForClientAuthAndCertManagement));
  if (system_key_slot_use_type_ ==
      SystemKeySlotUseType::kUseForClientAuthAndCertManagement) {
    EnableNSSSystemKeySlotForResourceContext(resource_context_.get());
  }

  certificate_provider_ = std::move(profile_params_->certificate_provider);
#endif

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    net::URLRequestContextBuilder builder;
    std::vector<std::unique_ptr<net::URLRequestInterceptor>>
        url_request_interceptors;
    url_request_interceptors.emplace_back(
        std::make_unique<FailingURLRequestInterceptor>());
    builder.SetInterceptors(std::move(url_request_interceptors));
    builder.set_network_quality_estimator(
        io_thread_globals->deprecated_network_quality_estimator.get());
    builder.set_proxy_resolution_service(
        net::ProxyResolutionService::CreateDirect());
    builder.SetCertVerifier(
        std::make_unique<WrappedCertVerifierForProfileIODataTesting>());
    main_request_context_owner_ =
        network::URLRequestContextOwner(nullptr, builder.Build());
    main_request_context_ =
        main_request_context_owner_.url_request_context.get();
  } else {
    // Create the main request context.
    std::unique_ptr<network::URLRequestContextBuilderMojo> builder =
        std::make_unique<network::URLRequestContextBuilderMojo>();

    std::unique_ptr<ChromeNetworkDelegate> chrome_network_delegate(
        new ChromeNetworkDelegate(
#if BUILDFLAG(ENABLE_EXTENSIONS)
            io_thread_globals->extension_event_router_forwarder.get()));
#else
            nullptr));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
    chrome_network_delegate->set_extension_info_map(
        profile_params_->extension_info_map.get());
#endif

    chrome_network_delegate->set_profile(profile_params_->profile);
    chrome_network_delegate->set_profile_path(profile_params_->path);
    chrome_network_delegate->set_cookie_settings(
        profile_params_->cookie_settings.get());

    chrome_network_delegate_unowned_ = chrome_network_delegate.get();

    std::unique_ptr<net::NetworkDelegate> network_delegate =
        ConfigureNetworkDelegate(profile_params_->io_thread,
                                 std::move(chrome_network_delegate));

    builder->set_network_delegate(std::move(network_delegate));

    io_thread->SetUpProxyService(builder.get());

    if (g_cert_verifier_for_profile_io_data_testing) {
      builder->SetCertVerifier(
          std::make_unique<WrappedCertVerifierForProfileIODataTesting>());
    } else {
      std::unique_ptr<net::CertVerifier> cert_verifier;
#if defined(OS_CHROMEOS)
      crypto::ScopedPK11Slot public_slot =
          crypto::GetPublicSlotForChromeOSUser(username_hash_);
      // The private slot won't be ready by this point. It shouldn't be
      // necessary for cert trust purposes anyway.
      scoped_refptr<net::CertVerifyProc> verify_proc(
          new network::CertVerifyProcChromeOS(std::move(public_slot)));
      if (profile_params_->policy_cert_verifier) {
        profile_params_->policy_cert_verifier->InitializeOnIOThread(
            verify_proc);
        cert_verifier = std::move(profile_params_->policy_cert_verifier);
      } else {
        cert_verifier = std::make_unique<net::CachingCertVerifier>(
            std::make_unique<net::MultiThreadedCertVerifier>(
                verify_proc.get()));
      }
#elif defined(OS_LINUX) || defined(OS_MACOSX)
      cert_verifier = std::make_unique<net::CachingCertVerifier>(
          std::make_unique<TrialComparisonCertVerifier>(
              profile_params_->profile, net::CertVerifyProc::CreateDefault(),
              net::CreateCertVerifyProcBuiltin()));
#else
      cert_verifier = std::make_unique<net::CachingCertVerifier>(
          std::make_unique<net::MultiThreadedCertVerifier>(
              net::CertVerifyProc::CreateDefault()));
#endif
      const base::CommandLine& command_line =
          *base::CommandLine::ForCurrentProcess();
      cert_verifier = network::IgnoreErrorsCertVerifier::MaybeWrapCertVerifier(
          command_line, switches::kUserDataDir, std::move(cert_verifier));
      builder->SetCertVerifier(std::move(cert_verifier));
    }

    // Install the New Tab Page Interceptor.
    if (profile_params_->new_tab_page_interceptor.get()) {
      request_interceptors.push_back(
          std::move(profile_params_->new_tab_page_interceptor));
    }

    if (data_reduction_proxy_io_data_.get()) {
      builder->set_shared_proxy_delegate(
          data_reduction_proxy_io_data_->proxy_delegate());
    }

    InitializeInternal(builder.get(), profile_params_.get(), protocol_handlers,
                       std::move(request_interceptors));

    builder->SetCreateHttpTransactionFactoryCallback(
        base::BindOnce(&content::CreateDevToolsNetworkTransactionFactory));

    main_network_context_ =
        content::GetNetworkServiceImpl()->CreateNetworkContextWithBuilder(
            std::move(profile_params_->main_network_context_request),
            std::move(profile_params_->main_network_context_params),
            std::move(builder), &main_request_context_);

    if (chrome_network_delegate_unowned_->domain_reliability_monitor()) {
      // Save a pointer to shut down Domain Reliability cleanly before the
      // URLRequestContext is dismantled.
      domain_reliability_monitor_unowned_ =
          chrome_network_delegate_unowned_->domain_reliability_monitor();

      domain_reliability_monitor_unowned_->InitURLRequestContext(
          main_request_context_);
      domain_reliability_monitor_unowned_->AddBakedInConfigs();
      domain_reliability_monitor_unowned_->SetDiscardUploads(
          !GetMetricsEnabledStateOnIOThread());
    }

    resource_context_->request_context_ = main_request_context_;
  }

  OnMainRequestContextCreated(profile_params_.get());

  profile_params_.reset();
  initialized_ = true;
}

std::unique_ptr<net::URLRequestJobFactory>
ProfileIOData::SetUpJobFactoryDefaults(
    std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory,
    content::URLRequestInterceptorScopedVector request_interceptors,
    std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor,
    net::NetworkDelegate* network_delegate,
    net::HostResolver* host_resolver) const {
  // NOTE(willchan): Keep these protocol handlers in sync with
  // ProfileIOData::IsHandledProtocol().
  bool set_protocol = job_factory->SetProtocolHandler(
      url::kFileScheme,
      std::make_unique<net::FileProtocolHandler>(
          base::CreateTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})));
  DCHECK(set_protocol);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK(extension_info_map_.get());
  // Check only for incognito (and not Chrome OS guest mode GUEST_PROFILE).
  bool is_incognito = profile_type() == Profile::INCOGNITO_PROFILE;
  set_protocol = job_factory->SetProtocolHandler(
      extensions::kExtensionScheme,
      extensions::CreateExtensionProtocolHandler(is_incognito,
                                                 extension_info_map_.get()));
  DCHECK(set_protocol);
#endif
  set_protocol = job_factory->SetProtocolHandler(
      url::kDataScheme, std::make_unique<net::DataProtocolHandler>());
  DCHECK(set_protocol);
#if defined(OS_CHROMEOS)
  if (profile_params_) {
    set_protocol = job_factory->SetProtocolHandler(
        content::kExternalFileScheme,
        std::make_unique<chromeos::ExternalFileProtocolHandler>(
            profile_params_->profile));
    DCHECK(set_protocol);
  }
#endif  // defined(OS_CHROMEOS)
#if defined(OS_ANDROID)
  set_protocol = job_factory->SetProtocolHandler(
      url::kContentScheme,
      content::ContentProtocolHandler::Create(base::CreateTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})));
#endif

  job_factory->SetProtocolHandler(
      url::kAboutScheme,
      std::make_unique<about_handler::AboutProtocolHandler>());

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  job_factory->SetProtocolHandler(
      url::kFtpScheme, net::FtpProtocolHandler::Create(host_resolver));
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

  // Set up interceptors in the reverse order.
  std::unique_ptr<net::URLRequestJobFactory> top_job_factory =
      std::move(job_factory);
  for (auto i = request_interceptors.rbegin(); i != request_interceptors.rend();
       ++i) {
    top_job_factory.reset(new net::URLRequestInterceptingJobFactory(
        std::move(top_job_factory), std::move(*i)));
  }
  request_interceptors.clear();

  if (protocol_handler_interceptor) {
    protocol_handler_interceptor->Chain(std::move(top_job_factory));
    return std::move(protocol_handler_interceptor);
  } else {
    return top_job_factory;
  }
}

void ProfileIOData::SetUpJobFactoryDefaultsForBuilder(
    net::URLRequestContextBuilder* builder,
    content::URLRequestInterceptorScopedVector request_interceptors,
    std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor) const {
// NOTE(willchan): Keep these protocol handlers in sync with
// ProfileIOData::IsHandledProtocol().
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK(extension_info_map_.get());
  // Check only for incognito (and not Chrome OS guest mode GUEST_PROFILE).
  bool is_incognito = profile_type() == Profile::INCOGNITO_PROFILE;
  builder->SetProtocolHandler(extensions::kExtensionScheme,
                              extensions::CreateExtensionProtocolHandler(
                                  is_incognito, extension_info_map_.get()));
#endif
#if defined(OS_CHROMEOS)
  if (profile_params_) {
    builder->SetProtocolHandler(
        content::kExternalFileScheme,
        std::make_unique<chromeos::ExternalFileProtocolHandler>(
            profile_params_->profile));
  }
#endif  // defined(OS_CHROMEOS)
#if defined(OS_ANDROID)
  builder->SetProtocolHandler(
      url::kContentScheme,
      content::ContentProtocolHandler::Create(base::CreateTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})));
#endif

  builder->SetProtocolHandler(
      url::kAboutScheme,
      std::make_unique<about_handler::AboutProtocolHandler>());

  builder->SetInterceptors(std::move(request_interceptors));

  if (protocol_handler_interceptor) {
    builder->set_create_intercepting_job_factory(base::BindOnce(
        &CreateURLRequestJobFactory, std::move(protocol_handler_interceptor)));
  }
}

void ProfileIOData::ShutdownOnUIThread(
    std::unique_ptr<ChromeURLRequestContextGetterVector> context_getters) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  google_services_user_account_id_.Destroy();
  sync_suppress_start_.Destroy();
  sync_first_setup_complete_.Destroy();
  sync_has_auth_error_.Destroy();
#if !defined(OS_CHROMEOS)
  signin_scoped_device_id_.Destroy();
#endif
  force_google_safesearch_.Destroy();
  force_youtube_restrict_.Destroy();
  allowed_domains_for_apps_.Destroy();
  enable_metrics_.Destroy();
  safe_browsing_enabled_.Destroy();
  safe_browsing_whitelist_domains_.Destroy();
  network_prediction_options_.Destroy();
  incognito_availibility_pref_.Destroy();
#if BUILDFLAG(ENABLE_PLUGINS)
  always_open_pdf_externally_.Destroy();
#endif
#if defined(OS_CHROMEOS)
  account_consistency_mirror_required_pref_.Destroy();
#endif

  if (!context_getters->empty()) {
    if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&NotifyContextGettersOfShutdownOnIO,
                         std::move(context_getters)));
    }
  }

  bool posted = BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this);
  if (!posted)
    delete this;
}

void ProfileIOData::DestroyResourceContext() {
  resource_context_.reset();
}

std::unique_ptr<net::HttpCache> ProfileIOData::CreateHttpFactory(
    net::HttpTransactionFactory* main_http_factory,
    std::unique_ptr<net::HttpCache::BackendFactory> backend) const {
  DCHECK(main_http_factory);
  net::HttpNetworkSession* shared_session = main_http_factory->GetSession();
  return std::make_unique<net::HttpCache>(
      content::CreateDevToolsNetworkTransactionFactory(shared_session),
      std::move(backend), false /* is_main_cache */);
}

void ProfileIOData::MaybeDeleteMediaCache(
    const base::FilePath& media_cache_path) {
  if (!base::FeatureList::IsEnabled(features::kUseSameCacheForMedia) ||
      media_cache_path.empty()) {
    return;
  }
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), media_cache_path,
                     true /* recursive */));
}

std::unique_ptr<net::NetworkDelegate> ProfileIOData::ConfigureNetworkDelegate(
    IOThread* io_thread,
    std::unique_ptr<ChromeNetworkDelegate> chrome_network_delegate) const {
  return std::move(chrome_network_delegate);
}
