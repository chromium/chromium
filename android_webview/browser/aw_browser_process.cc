// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_process.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_enterprise_authentication_app_link_manager.h"
#include "android_webview/browser/component_updater/registration.h"
#include "android_webview/browser/lifecycle/aw_contents_lifecycle_notifier.h"
#include "android_webview/browser/metrics/visibility_metrics_logger.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/base_paths_posix.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "components/crash/core/common/crash_key.h"
#include "components/embedder_support/origin_trials/origin_trials_settings_storage.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/process_visibility_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwBrowserProcess_jni.h"

using content::BrowserThread;

namespace android_webview {

namespace prefs {

// String that specifies the Android account type to use for Negotiate
// authentication.
const char kAuthAndroidNegotiateAccountType[] =
    "auth.android_negotiate_account_type";

// Allowlist containing servers for which Integrated Authentication is enabled.
// This pref should match |prefs::kAuthServerAllowlist|.
const char kAuthServerAllowlist[] = "auth.server_allowlist";

// This pref contains a list of authentication urls, for which when webview is
// navigated to any of these urls, browse intent will be sent.
const char kEnterpriseAuthAppLinkPolicy[] = "enterprise_auth_app_link_policy";

}  // namespace prefs

namespace {
AwBrowserProcess* g_aw_browser_process = nullptr;
}  // namespace

// static
AwBrowserProcess* AwBrowserProcess::GetInstance() {
  return g_aw_browser_process;
}

AwBrowserProcess::AwBrowserProcess(
    AwFeatureListCreator* aw_feature_list_creator) {
  g_aw_browser_process = this;
  aw_feature_list_creator_ = aw_feature_list_creator;
  aw_contents_lifecycle_notifier_ =
      std::make_unique<AwContentsLifecycleNotifier>(base::BindRepeating(
          &AwBrowserProcess::OnLoseForeground, base::Unretained(this)));

  app_link_manager_ =
      std::make_unique<EnterpriseAuthenticationAppLinkManager>(local_state());

  origin_trials_settings_storage_ =
      std::make_unique<embedder_support::OriginTrialsSettingsStorage>();

  // Initialize OSCryptAsync with no providers. This delegates all encryption
  // operations to OSCrypt.
  os_crypt_async_ = std::make_unique<os_crypt_async::OSCryptAsync>(
      std::vector<
          std::pair<size_t, std::unique_ptr<os_crypt_async::KeyProvider>>>());
}

AwBrowserProcess::~AwBrowserProcess() {
  g_aw_browser_process = nullptr;
}

void AwBrowserProcess::PreMainMessageLoopRun() {
  pref_change_registrar_.Init(local_state());
  auto auth_pref_callback = base::BindRepeating(
      &AwBrowserProcess::OnAuthPrefsChanged, base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAuthServerAllowlist, auth_pref_callback);
  pref_change_registrar_.Add(prefs::kAuthAndroidNegotiateAccountType,
                             auth_pref_callback);

  // Trigger async initialization of OSCrypt key providers.
  std::ignore = os_crypt_async_->GetInstance(base::DoNothing());

  InitSafeBrowsing();
}

PrefService* AwBrowserProcess::local_state() {
  if (!local_state_)
    CreateLocalState();
  return local_state_.get();
}

void AwBrowserProcess::CreateLocalState() {
  DCHECK(!local_state_);

  local_state_ = aw_feature_list_creator_->TakePrefService();
  DCHECK(local_state_);
}

void AwBrowserProcess::OnLoseForeground() {
  if (local_state_)
    local_state_->CommitPendingWrite();
}

AwBrowserPolicyConnector* AwBrowserProcess::browser_policy_connector() {
  if (!browser_policy_connector_)
    CreateBrowserPolicyConnector();
  return browser_policy_connector_.get();
}

VisibilityMetricsLogger* AwBrowserProcess::visibility_metrics_logger() {
  if (!visibility_metrics_logger_) {
    visibility_metrics_logger_ = std::make_unique<VisibilityMetricsLogger>();

    visibility_metrics_logger_->SetOnVisibilityChangedCallback(
        base::BindRepeating([](bool visible) {
          content::OnBrowserVisibilityChanged(visible);
        }));
  }
  return visibility_metrics_logger_.get();
}

void AwBrowserProcess::CreateBrowserPolicyConnector() {
  DCHECK(!browser_policy_connector_);

  browser_policy_connector_ =
      aw_feature_list_creator_->TakeBrowserPolicyConnector();
  DCHECK(browser_policy_connector_);
}

void AwBrowserProcess::InitSafeBrowsing() {
  CreateSafeBrowsingUIManager();
  CreateSafeBrowsingAllowlistManager();
}

void AwBrowserProcess::CreateSafeBrowsingUIManager() {
  safe_browsing_ui_manager_ = new AwSafeBrowsingUIManager();
}

void AwBrowserProcess::CreateSafeBrowsingAllowlistManager() {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      content::GetIOThreadTaskRunner({});
  safe_browsing_allowlist_manager_ =
      std::make_unique<AwSafeBrowsingAllowlistManager>(background_task_runner,
                                                       io_task_runner);
}

safe_browsing::RemoteSafeBrowsingDatabaseManager*
AwBrowserProcess::GetSafeBrowsingDBManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!safe_browsing_db_manager_) {
    safe_browsing_db_manager_ =
        new safe_browsing::RemoteSafeBrowsingDatabaseManager();
  }

  if (!safe_browsing_db_manager_started_) {
    // V4ProtocolConfig is not used. Just create one with empty values..
    safe_browsing::V4ProtocolConfig config("", false, "", "");
    safe_browsing_db_manager_->StartOnUIThread(
        GetSafeBrowsingUIManager()->GetURLLoaderFactory(), config);
    safe_browsing_db_manager_started_ = true;
  }

  return safe_browsing_db_manager_.get();
}

safe_browsing::TriggerManager*
AwBrowserProcess::GetSafeBrowsingTriggerManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!safe_browsing_trigger_manager_) {
    safe_browsing_trigger_manager_ =
        std::make_unique<safe_browsing::TriggerManager>(
            GetSafeBrowsingUIManager(),
            /*local_state_prefs=*/nullptr);
  }

  return safe_browsing_trigger_manager_.get();
}

AwSafeBrowsingAllowlistManager*
AwBrowserProcess::GetSafeBrowsingAllowlistManager() const {
  return safe_browsing_allowlist_manager_.get();
}

AwSafeBrowsingUIManager* AwBrowserProcess::GetSafeBrowsingUIManager() const {
  return safe_browsing_ui_manager_.get();
}

os_crypt_async::OSCryptAsync* AwBrowserProcess::GetOSCryptAsync() const {
  return os_crypt_async_.get();
}

// static
void AwBrowserProcess::RegisterNetworkContextLocalStatePrefs(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterStringPref(prefs::kAuthServerAllowlist, std::string());
  pref_registry->RegisterStringPref(prefs::kAuthAndroidNegotiateAccountType,
                                    std::string());
}

void AwBrowserProcess::RegisterEnterpriseAuthenticationAppLinkPolicyPref(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterListPref(prefs::kEnterpriseAuthAppLinkPolicy);
}

network::mojom::HttpAuthDynamicParamsPtr
AwBrowserProcess::CreateHttpAuthDynamicParams() {
  network::mojom::HttpAuthDynamicParamsPtr auth_dynamic_params =
      network::mojom::HttpAuthDynamicParams::New();

  auth_dynamic_params->allowed_schemes = AwBrowserContext::GetAuthSchemes();
  auth_dynamic_params->server_allowlist =
      local_state()->GetString(prefs::kAuthServerAllowlist);
  auth_dynamic_params->android_negotiate_account_type =
      local_state()->GetString(prefs::kAuthAndroidNegotiateAccountType);

  auth_dynamic_params->ntlm_v2_enabled = true;

  return auth_dynamic_params;
}

void AwBrowserProcess::OnAuthPrefsChanged() {
  content::GetNetworkService()->ConfigureHttpAuthPrefs(
      CreateHttpAuthDynamicParams());
}

EnterpriseAuthenticationAppLinkManager*
AwBrowserProcess::GetEnterpriseAuthenticationAppLinkManager() {
  return app_link_manager_.get();
}

embedder_support::OriginTrialsSettingsStorage*
AwBrowserProcess::GetOriginTrialsSettingsStorage() {
  return origin_trials_settings_storage_.get();
}

// static
void AwBrowserProcess::TriggerMinidumpUploading() {
  Java_AwBrowserProcess_triggerMinidumpUploading(
      base::android::AttachCurrentThread());
}

// static
ApkType AwBrowserProcess::GetApkType() {
  return static_cast<ApkType>(
      Java_AwBrowserProcess_getApkType(base::android::AttachCurrentThread()));
}

static void JNI_AwBrowserProcess_SetProcessNameCrashKey(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& processName) {
  static ::crash_reporter::CrashKeyString<64> crash_key(
      crash_keys::kAppProcessName);
  crash_key.Set(base::android::ConvertJavaStringToUTF8(env, processName));
}

static base::android::ScopedJavaLocalRef<jobjectArray>
JNI_AwBrowserProcess_GetComponentLoaderPolicies(JNIEnv* env) {
  return component_updater::AndroidComponentLoaderPolicy::
      ToJavaArrayOfAndroidComponentLoaderPolicy(env,
                                                GetComponentLoaderPolicies());
}

}  // namespace android_webview
