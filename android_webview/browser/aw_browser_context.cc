// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "android_webview/browser/aw_browser_context_store.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_client_hints_controller_delegate.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_contents_origin_matcher.h"
#include "android_webview/browser/aw_download_manager_delegate.h"
#include "android_webview/browser/aw_form_database_service.h"
#include "android_webview/browser/aw_permission_manager.h"
#include "android_webview/browser/aw_quota_manager_bridge.h"
#include "android_webview/browser/aw_web_ui_controller_factory.h"
#include "android_webview/browser/cookie_manager.h"
#include "android_webview/browser/ip_protection/aw_ip_protection_core_host.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/network_service/net_helpers.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_allowlist_manager.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/base_paths_posix.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/origin_trials/browser/leveldb_persistence_provider.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/origin_trials/common/features.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_name_set.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/segregated_pref_store.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_prefs/user_prefs.h"
#include "components/visitedlink/browser/visitedlink_writer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/zoom_level_delegate.h"
#include "media/mojo/buildflags.h"
#include "net/base/features.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_config_service_android.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwBrowserContext_jni.h"

using base::FilePath;
using content::BrowserThread;

namespace android_webview {

namespace {

const void* const kDownloadManagerDelegateKey = &kDownloadManagerDelegateKey;

// Empty method to skip origin security check as DownloadManager will set its
// own method.
bool IgnoreOriginSecurityCheck(const GURL& url) {
  return true;
}

void MigrateProfileData(base::FilePath cache_path,
                        base::FilePath context_storage_path) {
  TRACE_EVENT0("startup", "MigrateProfileData");
  FilePath old_cache_path;
  base::PathService::Get(base::DIR_CACHE, &old_cache_path);
  old_cache_path = old_cache_path.DirName().Append(
      FILE_PATH_LITERAL("org.chromium.android_webview"));

  if (base::PathExists(old_cache_path)) {
    bool success = base::CreateDirectory(cache_path);
    if (success)
      success &= base::Move(old_cache_path, cache_path);
    DCHECK(success);
  }

  base::FilePath old_context_storage_path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &old_context_storage_path);

  if (!base::PathExists(context_storage_path)) {
    base::CreateDirectory(context_storage_path);
  }

  auto migrate_context_storage_data = [&old_context_storage_path,
                                       &context_storage_path](auto& suffix) {
    FilePath old_file = old_context_storage_path.Append(suffix);
    if (base::PathExists(old_file)) {
      FilePath new_file = context_storage_path.Append(suffix);

      if (base::PathExists(new_file)) {
        bool success =
            base::Move(new_file, new_file.AddExtension(".partial-migration"));
        DCHECK(success);
      }
      bool success = base::Move(old_file, new_file);
      DCHECK(success);
    }
  };

  // These were handled in the initial migration
  migrate_context_storage_data("Web Data");
  migrate_context_storage_data("Web Data-journal");
  migrate_context_storage_data("GPUCache");
  migrate_context_storage_data("blob_storage");
  migrate_context_storage_data("Session Storage");

  // These were missed in the initial migration
  migrate_context_storage_data("File System");
  migrate_context_storage_data("IndexedDB");
  migrate_context_storage_data("Local Storage");
  migrate_context_storage_data("QuotaManager");
  migrate_context_storage_data("QuotaManager-journal");
  migrate_context_storage_data("Service Worker");
  migrate_context_storage_data("VideoDecodeStats");
  migrate_context_storage_data("databases");
  migrate_context_storage_data("shared_proto_db");
  migrate_context_storage_data("webrtc_event_logs");
}

base::FilePath BuildCachePath(const base::FilePath& relative_path) {
  FilePath cache_path;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_path)) {
    NOTREACHED() << "Failed to get app cache directory for Android WebView";
  }
  return cache_path.Append(relative_path);
}

base::FilePath BuildHttpCachePath(const base::FilePath& relative_path) {
  return BuildCachePath(relative_path).Append(FILE_PATH_LITERAL("HTTP Cache"));
}

}  // namespace

AwBrowserContext::AwBrowserContext(std::string name,
                                   base::FilePath relative_path,
                                   const bool is_default)
    : name_(std::move(name)),
      relative_path_(std::move(relative_path)),
      is_default_(is_default),
      context_storage_path_(BuildStoragePath(relative_path_)),
      http_cache_path_(BuildHttpCachePath(relative_path_)),
      simple_factory_key_(GetPath(), IsOffTheRecord()),
      service_worker_xrw_allowlist_matcher_(
          base::MakeRefCounted<AwContentsOriginMatcher>()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT0("startup", "AwBrowserContext::AwBrowserContext");

  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kRegular);

  if (IsDefaultBrowserContext()) {
    MigrateProfileData(GetHttpCachePath(), GetPath());
  } else {
    cookie_manager_ = std::make_unique<CookieManager>(this);
  }

  SimpleKeyMap::GetInstance()->Associate(this, &simple_factory_key_);

  CreateUserPrefService();

  visitedlink_writer_ =
      std::make_unique<visitedlink::VisitedLinkWriter>(this, this, false);
  visitedlink_writer_->Init();

  form_database_service_ =
      std::make_unique<AwFormDatabaseService>(context_storage_path_);

  EnsureResourceContextInitialized();
}

AwBrowserContext::~AwBrowserContext() {
  NotifyWillBeDestroyed();
  SimpleKeyMap::GetInstance()->Dissociate(this);
  ShutdownStoragePartitions();
}

// static
AwBrowserContext* AwBrowserContext::GetDefault() {
  return AwBrowserContextStore::GetInstance()->GetDefault();
}

// static
AwBrowserContext* AwBrowserContext::FromWebContents(
    content::WebContents* web_contents) {
  // This cast is safe; this is the only implementation of the browser context.
  return static_cast<AwBrowserContext*>(web_contents->GetBrowserContext());
}

base::FilePath AwBrowserContext::GetHttpCachePath() {
  return http_cache_path_;
}

base::FilePath AwBrowserContext::GetPrefStorePath() {
  return GetPath().Append(FILE_PATH_LITERAL("Preferences"));
}

base::FilePath AwBrowserContext::GetCookieStorePath() {
  return GetCookieManager()->GetCookieStorePath();
}

base::android::ScopedJavaLocalRef<jobjectArray>
AwBrowserContext::UpdateServiceWorkerXRequestedWithAllowListOriginMatcher(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& jrules) {
  std::vector<std::string> rules;
  base::android::AppendJavaStringArrayToStringVector(env, jrules, &rules);
  std::vector<std::string> bad_rules =
      service_worker_xrw_allowlist_matcher_->UpdateRuleList(rules);
  return base::android::ToJavaArrayOfStrings(env, bad_rules);
}

// static
void AwBrowserContext::RegisterPrefs(PrefRegistrySimple* registry) {
  safe_browsing::RegisterProfilePrefs(registry);

  // Register the Autocomplete Data Retention Policy pref.
  // The default value '0' represents the latest Chrome major version on which
  // the retention policy ran. By setting it to a low default value, we're
  // making sure it runs now (as it only runs once per major version).
  registry->RegisterIntegerPref(
      autofill::prefs::kAutocompleteLastVersionRetentionPolicy, 0);

  // We only use the autocomplete feature of Autofill, which is controlled via
  // the manager_delegate. We don't use the rest of Autofill, which is why it is
  // hardcoded as disabled here.
  // TODO(crbug.com/40589187): The following also disables autocomplete.
  // Investigate what the intended behavior is.
  registry->RegisterBooleanPref(autofill::prefs::kAutofillProfileEnabled,
                                false);
  registry->RegisterBooleanPref(autofill::prefs::kAutofillCreditCardEnabled,
                                false);

  // This contains a map from a given origin to the client hint headers
  // requested to be sent next time that origin is loaded.
  registry->RegisterDictionaryPref(prefs::kClientHintsCachedPerOriginMap);

#if BUILDFLAG(ENABLE_MOJO_CDM)
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(registry);
#endif
}

void AwBrowserContext::CreateUserPrefService() {
  TRACE_EVENT0("startup", "AwBrowserContext::CreateUserPrefService");
  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();

  RegisterPrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;

  PrefNameSet persistent_prefs;
  // Persisted to avoid having to provision MediaDrm every time the
  // application tries to play protected content after restart.
  persistent_prefs.insert(cdm::prefs::kMediaDrmStorage);
  // Persisted to ensure client hints can be sent on next page load.
  persistent_prefs.insert(prefs::kClientHintsCachedPerOriginMap);

  pref_service_factory.set_user_prefs(base::MakeRefCounted<SegregatedPrefStore>(
      base::MakeRefCounted<InMemoryPrefStore>(),
      base::MakeRefCounted<JsonPrefStore>(GetPrefStorePath()),
      std::move(persistent_prefs)));

  policy::URLBlocklistManager::RegisterProfilePrefs(pref_registry.get());
  AwBrowserPolicyConnector* browser_policy_connector =
      AwBrowserProcess::GetInstance()->browser_policy_connector();
  pref_service_factory.set_managed_prefs(
      base::MakeRefCounted<policy::ConfigurationPolicyPrefStore>(
          browser_policy_connector,
          browser_policy_connector->GetPolicyService(),
          browser_policy_connector->GetHandlerList(),
          policy::POLICY_LEVEL_MANDATORY));
  {
    // TODO(crbug.com/40268809): We can potentially use
    // pref_service_factory.set_async(true) instead of ScopedAllowBlocking in
    // order to avoid blocking here or to at least parallelize work in the
    // background, but it might require additional cross-thread synchronization.
    //
    // Note that for the default profile blocking IO is already permitted on the
    // UI thread due to being called during Chromium/browser
    // initialization. ScopedAllowBlocking is explicitly needed for non-default
    // profiles as they are instead created from a calling environment where
    // normal threading restrictions apply.
    base::ScopedAllowBlocking scoped_allow_blocking;
    user_pref_service_ = pref_service_factory.Create(pref_registry);
  }

  if (IsDefaultBrowserContext()) {
    MigrateLocalStatePrefs();
  }

  user_prefs::UserPrefs::Set(this, user_pref_service_.get());
}

void AwBrowserContext::MigrateLocalStatePrefs() {
  PrefService* local_state = AwBrowserProcess::GetInstance()->local_state();
  if (!local_state->HasPrefPath(cdm::prefs::kMediaDrmStorage)) {
    return;
  }

  user_pref_service_->Set(cdm::prefs::kMediaDrmStorage,
                          local_state->GetValue(cdm::prefs::kMediaDrmStorage));
  local_state->ClearPref(cdm::prefs::kMediaDrmStorage);
}

// static
std::vector<std::string> AwBrowserContext::GetAuthSchemes() {
  // In Chrome this is configurable via the AuthSchemes policy. For WebView
  // there is no interest to have it available so far.
  std::vector<std::string> supported_schemes = {"basic", "digest", "ntlm",
                                                "negotiate"};
  return supported_schemes;
}

void AwBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  DCHECK(visitedlink_writer_);
  visitedlink_writer_->AddURLs(urls);
}

AwQuotaManagerBridge* AwBrowserContext::GetQuotaManagerBridge() {
  if (!quota_manager_bridge_.get()) {
    quota_manager_bridge_ = AwQuotaManagerBridge::Create(this);
  }
  return quota_manager_bridge_.get();
}

AwFormDatabaseService* AwBrowserContext::GetFormDatabaseService() {
  return form_database_service_.get();
}

CookieManager* AwBrowserContext::GetCookieManager() {
  if (IsDefaultBrowserContext()) {
    // For the default context, the CookieManager isn't owned by the context,
    // and may be initialized externally.
    CHECK(!cookie_manager_);
    return CookieManager::GetDefaultInstance();
  } else {
    // Non-default contexts own their cookie managers
    CHECK(cookie_manager_);
    return cookie_manager_.get();
  }
}

bool AwBrowserContext::IsDefaultBrowserContext() const {
  return is_default_;
}

base::FilePath AwBrowserContext::GetPath() {
  return context_storage_path_;
}

bool AwBrowserContext::IsOffTheRecord() {
  // Android WebView does not support off the record profile yet.
  return false;
}

content::DownloadManagerDelegate*
AwBrowserContext::GetDownloadManagerDelegate() {
  if (!GetUserData(kDownloadManagerDelegateKey)) {
    SetUserData(kDownloadManagerDelegateKey,
                std::make_unique<AwDownloadManagerDelegate>());
  }
  return static_cast<AwDownloadManagerDelegate*>(
      GetUserData(kDownloadManagerDelegateKey));
}

content::BrowserPluginGuestManager* AwBrowserContext::GetGuestManager() {
  return NULL;
}

storage::SpecialStoragePolicy* AwBrowserContext::GetSpecialStoragePolicy() {
  // Intentionally returning NULL as 'Extensions' and 'Apps' not supported.
  return NULL;
}

content::PlatformNotificationService*
AwBrowserContext::GetPlatformNotificationService() {
  return nullptr;
}

content::PushMessagingService* AwBrowserContext::GetPushMessagingService() {
  // TODO(johnme): Support push messaging in WebView.
  return NULL;
}

content::StorageNotificationService*
AwBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate* AwBrowserContext::GetSSLHostStateDelegate() {
  if (!ssl_host_state_delegate_.get()) {
    ssl_host_state_delegate_ = std::make_unique<AwSSLHostStateDelegate>();
  }
  return ssl_host_state_delegate_.get();
}

AwPermissionManager* AwBrowserContext::GetPermissionControllerDelegate() {
  if (!permission_manager_.get())
    permission_manager_ = std::make_unique<AwPermissionManager>(*this);
  return permission_manager_.get();
}

content::ClientHintsControllerDelegate*
AwBrowserContext::GetClientHintsControllerDelegate() {
  if (!client_hints_controller_delegate_.get()) {
    client_hints_controller_delegate_ =
        std::make_unique<AwClientHintsControllerDelegate>(GetPrefService());
  }
  return client_hints_controller_delegate_.get();
}

content::BackgroundFetchDelegate*
AwBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
AwBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
AwBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

content::FileSystemAccessPermissionContext*
AwBrowserContext::GetFileSystemAccessPermissionContext() {
  return &fsa_permission_context_;
}

content::ReduceAcceptLanguageControllerDelegate*
AwBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  return nullptr;
}

std::unique_ptr<download::InProgressDownloadManager>
AwBrowserContext::RetrieveInProgressDownloadManager() {
  return std::make_unique<download::InProgressDownloadManager>(
      nullptr, base::FilePath(), nullptr,
      base::BindRepeating(&IgnoreOriginSecurityCheck),
      base::BindRepeating(&content::DownloadRequestUtils::IsURLSafe),
      /*wake_lock_provider_binder*/ base::NullCallback());
}

content::OriginTrialsControllerDelegate*
AwBrowserContext::GetOriginTrialsControllerDelegate() {
  if (!origin_trials::features::IsPersistentOriginTrialsEnabled())
    return nullptr;

  if (!origin_trials_controller_delegate_) {
    origin_trials_controller_delegate_ =
        std::make_unique<origin_trials::OriginTrials>(
            std::make_unique<origin_trials::LevelDbPersistenceProvider>(
                GetPath(),
                GetDefaultStoragePartition()->GetProtoDatabaseProvider()),
            std::make_unique<blink::TrialTokenValidator>());
  }
  return origin_trials_controller_delegate_.get();
}

std::unique_ptr<content::ZoomLevelDelegate>
AwBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}

void AwBrowserContext::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  // Android WebView rebuilds from WebChromeClient.getVisitedHistory. The client
  // can change in the lifetime of this WebView and may not yet be set here.
  // Therefore this initialization path is not used.
  enumerator->OnComplete(true);
}

void AwBrowserContext::BuildVisitedLinkTable(
    const scoped_refptr<VisitedLinkEnumerator>& enumerator) {
  // Partitioned visited link hashtables are not supported in Android WebView,
  // so this initialization path is not used.
  enumerator->OnVisitedLinkComplete(true);
}

void AwBrowserContext::SetExtendedReportingAllowed(bool allowed) {
  user_pref_service_->SetBoolean(
      ::prefs::kSafeBrowsingExtendedReportingOptInAllowed, allowed);
}

// TODO(amalova): Make sure NetworkContextParams is configured correctly when
// off-the-record
void AwBrowserContext::ConfigureNetworkContextParams(
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  context_params->user_agent = android_webview::GetUserAgent();

  // TODO(ntfschr): set this value to a proper value based on the user's
  // preferred locales (http://crbug.com/898555). For now, set this to
  // "en-US,en" instead of "en-us,en", since Android guarantees region codes
  // will be uppercase.
  context_params->accept_language =
      net::HttpUtil::GenerateAcceptLanguageHeader("en-US,en");

  // HTTP cache
  context_params->http_cache_enabled = true;
  context_params->http_cache_max_size = GetHttpCacheSize();

  // WebView should persist and restore cookies between app sessions (including
  // session cookies).
  context_params->file_paths = network::mojom::NetworkContextFilePaths::New();
  // Adding HTTP cache dir here
  context_params->file_paths->http_cache_directory = GetHttpCachePath();
  base::FilePath cookie_path = AwBrowserContext::GetCookieStorePath();
  context_params->file_paths->data_directory = cookie_path.DirName();
  context_params->file_paths->cookie_database_name = cookie_path.BaseName();
  context_params->restore_old_session_cookies = true;
  context_params->persist_session_cookies = true;
  context_params->cookie_manager_params =
      network::mojom::CookieManagerParams::New();
  context_params->cookie_manager_params->allow_file_scheme_cookies =
      GetCookieManager()->GetAllowFileSchemeCookies();
  context_params->cookie_manager_params->cookie_access_delegate_type =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewEnableModernCookieSameSite)
          ? network::mojom::CookieAccessDelegateType::ALWAYS_NONLEGACY
          : network::mojom::CookieAccessDelegateType::ALWAYS_LEGACY;

  context_params->initial_ssl_config = network::mojom::SSLConfig::New();
  // Allow SHA-1 to be used for locally-installed trust anchors, as WebView
  // should behave like the Android system would.
  context_params->initial_ssl_config->sha1_local_anchors_enabled = true;
  // Do not enforce the Legacy Symantec PKI policies outlined in
  // https://security.googleblog.com/2017/09/chromes-plan-to-distrust-symantec.html,
  // defer to the Android system.
  context_params->initial_ssl_config->symantec_enforcement_disabled = true;

  // WebView does not currently support Certificate Transparency
  // (http://crbug.com/921750).
  context_params->enforce_chrome_ct_policy = false;

  context_params->enable_brotli = true;
  context_params->enable_zstd =
      base::FeatureList::IsEnabled(net::features::kZstdContentEncoding);

  context_params->check_clear_text_permitted =
      AwContentBrowserClient::get_check_cleartext_permitted();

  AwIpProtectionCoreHost* aw_ipp_core_host = AwIpProtectionCoreHost::Get(this);
  if (aw_ipp_core_host) {
    aw_ipp_core_host->AddNetworkService(
        context_params->ip_protection_config_getter
            .InitWithNewPipeAndPassReceiver(),
        context_params->ip_protection_control.InitWithNewPipeAndPassRemote());
    context_params->enable_ip_protection =
        aw_ipp_core_host->IsIpProtectionEnabled();
  }

  // Add proxy settings
  AwProxyConfigMonitor::GetInstance()->AddProxyToNetworkContextParams(
      context_params);
}

base::android::ScopedJavaLocalRef<jobject> JNI_AwBrowserContext_GetDefaultJava(
    JNIEnv* env) {
  AwBrowserContext* default_context = AwBrowserContext::GetDefault();
  CHECK(default_context);
  return default_context->GetJavaBrowserContext();
}

base::android::ScopedJavaLocalRef<jstring>
JNI_AwBrowserContext_GetDefaultContextName(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, AwBrowserContextStore::kDefaultContextName);
}

base::android::ScopedJavaLocalRef<jstring>
JNI_AwBrowserContext_GetDefaultContextRelativePath(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, AwBrowserContextStore::kDefaultContextPath);
}

void AwBrowserContext::ClearPersistentOriginTrialStorageForTesting(
    JNIEnv* env) {
  content::OriginTrialsControllerDelegate* delegate =
      GetOriginTrialsControllerDelegate();
  if (delegate)
    delegate->ClearPersistedTokens();
}

jboolean AwBrowserContext::HasFormData(JNIEnv* env) {
  return GetFormDatabaseService()->HasFormData();
}

void AwBrowserContext::ClearFormData(JNIEnv* env) {
  return GetFormDatabaseService()->ClearFormData();
}

base::android::ScopedJavaLocalRef<jobject>
AwBrowserContext::GetJavaBrowserContext() {
  if (!obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    obj_ = Java_AwBrowserContext_create(
        env, reinterpret_cast<intptr_t>(this),
        base::android::ConvertUTF8ToJavaString(env, name_),
        base::android::ConvertUTF8ToJavaString(env, relative_path_.value()),
        GetCookieManager()->GetJavaCookieManager(), IsDefaultBrowserContext());
  }
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

jlong AwBrowserContext::GetQuotaManagerBridge(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(GetQuotaManagerBridge());
}

scoped_refptr<AwContentsOriginMatcher>
AwBrowserContext::service_worker_xrw_allowlist_matcher() {
  return service_worker_xrw_allowlist_matcher_;
}

void AwBrowserContext::SetExtraHeaders(const GURL& url,
                                       const std::string& headers) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!url.is_valid()) {
    return;
  }
  if (!headers.empty()) {
    extra_headers_[url.spec()] = headers;
  } else {
    extra_headers_.erase(url.spec());
  }
}

std::string AwBrowserContext::GetExtraHeaders(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!url.is_valid()) {
    return std::string();
  }
  std::map<std::string, std::string>::iterator iter =
      extra_headers_.find(url.spec());
  return iter != extra_headers_.end() ? iter->second : std::string();
}

void AwBrowserContext::SetServiceWorkerIoThreadClient(
    JNIEnv* const env,
    const base::android::JavaParamRef<jobject>& io_thread_client) {
  sw_io_thread_client_ =
      base::android::ScopedJavaGlobalRef<jobject>(io_thread_client);
}

std::unique_ptr<AwContentsIoThreadClient>
AwBrowserContext::GetServiceWorkerIoThreadClientThreadSafe() {
  base::android::ScopedJavaLocalRef<jobject> java_delegate =
      base::android::ScopedJavaLocalRef<jobject>(sw_io_thread_client_);
  if (java_delegate) {
    return std::make_unique<AwContentsIoThreadClient>(java_delegate);
  }
  return nullptr;
}

// static
base::FilePath AwBrowserContext::BuildStoragePath(
    const base::FilePath& relative_path) {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &user_data_dir)) {
    NOTREACHED() << "Failed to get app data directory for Android WebView";
  }
  return user_data_dir.Append(relative_path);
}

// static
void AwBrowserContext::PrepareNewContext(const base::FilePath& relative_path) {
  base::ScopedAllowBlocking scoped_allow_blocking;
  const base::FilePath storage_path = BuildStoragePath(relative_path);
  bool storage_created = base::CreateDirectory(storage_path);
  CHECK(storage_created);
}

// static
void AwBrowserContext::DeleteContext(const base::FilePath& relative_path) {
  // The default profile handles its own directory creation in migration code
  // and (as of writing) should never be deleted.
  CHECK_NE(relative_path.value(), AwBrowserContextStore::kDefaultContextPath);

  // TODO(crbug.com/40268809): This could be partially backgrounded by deleting
  // on the thread pool. Ideally, any interrupted profile directory deletion
  // would be resumed in the background on startup. For now, this just deletes
  // synchronously.
  //
  // We probably also won't want to CHECK in the final solution, but perhaps
  // instead allow for some kind of retry-later logic.
  base::ScopedAllowBlocking scoped_allow_blocking;
  const base::FilePath storage_path = BuildStoragePath(relative_path);
  const base::FilePath cache_path = BuildCachePath(relative_path);
  bool storage_deleted = base::DeletePathRecursively(storage_path);
  CHECK(storage_deleted);
  bool cache_deleted = base::DeletePathRecursively(cache_path);
  CHECK(cache_deleted);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AwBrowserContext_deleteSharedPreferences(
      env, base::android::ConvertUTF8ToJavaString(env, relative_path.value()));
}
blink::mojom::PermissionStatus AwBrowserContext::GetGeolocationPermission(
    const GURL& origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!obj_) {
    return blink::mojom::PermissionStatus::ASK;
  }

  base::android::ScopedJavaLocalRef<jstring> j_origin(
      base::android::ConvertUTF8ToJavaString(env, origin.spec()));
  return static_cast<blink::mojom::PermissionStatus>(
      Java_AwBrowserContext_getGeolocationPermission(env, obj_, j_origin));
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
AwBrowserContext::CreateURLLoaderFactory() {
  auto url_loader_factory_params =
      network::mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
  url_loader_factory_params->is_orb_enabled = false;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory;

  GetDefaultStoragePartition()->GetNetworkContext()->CreateURLLoaderFactory(
      factory.InitWithNewPipeAndPassReceiver(),
      std::move(url_loader_factory_params));

  return factory;
}

}  // namespace android_webview
