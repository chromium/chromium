// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This interface is for managing the global services of the application.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PROCESS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PROCESS_H_

#include "android_webview/browser/aw_apk_type.h"
#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_enterprise_authentication_app_link_manager.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/browser/lifecycle/aw_contents_lifecycle_notifier.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_allowlist_manager.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/android/remote_database_manager.h"
#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "net/log/net_log.h"
#include "services/network/network_service.h"

namespace embedder_support {
class OriginTrialsSettingsStorage;
}  // namespace embedder_support

namespace android_webview {

namespace prefs {

// Used for Kerberos authentication.
extern const char kAuthAndroidNegotiateAccountType[];
extern const char kAuthServerAllowlist[];
extern const char kEnterpriseAuthAppLinkPolicy[];

}  // namespace prefs

class AwContentsLifecycleNotifier;
class VisibilityMetricsLogger;

// Lifetime: Singleton
class AwBrowserProcess {
 public:
  AwBrowserProcess(AwFeatureListCreator* aw_feature_list_creator);

  AwBrowserProcess(const AwBrowserProcess&) = delete;
  AwBrowserProcess& operator=(const AwBrowserProcess&) = delete;

  ~AwBrowserProcess();

  static AwBrowserProcess* GetInstance();

  PrefService* local_state();
  AwBrowserPolicyConnector* browser_policy_connector();
  VisibilityMetricsLogger* visibility_metrics_logger();

  void CreateBrowserPolicyConnector();
  void CreateLocalState();
  void InitSafeBrowsing();

  safe_browsing::RemoteSafeBrowsingDatabaseManager* GetSafeBrowsingDBManager();

  // Called on UI thread.
  // This method lazily creates TriggerManager.
  // Needs to happen after |safe_browsing_ui_manager_| is created.
  safe_browsing::TriggerManager* GetSafeBrowsingTriggerManager();

  // InitSafeBrowsing must be called first.
  // Called on UI and IO threads.
  AwSafeBrowsingAllowlistManager* GetSafeBrowsingAllowlistManager() const;

  // InitSafeBrowsing must be called first.
  // Called on UI and IO threads.
  AwSafeBrowsingUIManager* GetSafeBrowsingUIManager() const;

  // Obtain the browser instance of OSCryptAsync, which should be used for data
  // encryption.
  os_crypt_async::OSCryptAsync* GetOSCryptAsync() const;

  static void RegisterNetworkContextLocalStatePrefs(
      PrefRegistrySimple* pref_registry);
  static void RegisterEnterpriseAuthenticationAppLinkPolicyPref(
      PrefRegistrySimple* pref_registry);

  // Constructs HttpAuthDynamicParams based on |local_state_|.
  network::mojom::HttpAuthDynamicParamsPtr CreateHttpAuthDynamicParams();

  void PreMainMessageLoopRun();

  static void TriggerMinidumpUploading();
  static ApkType GetApkType();

  EnterpriseAuthenticationAppLinkManager*
  GetEnterpriseAuthenticationAppLinkManager();

  embedder_support::OriginTrialsSettingsStorage*
  GetOriginTrialsSettingsStorage();

 private:
  void CreateSafeBrowsingUIManager();
  void CreateSafeBrowsingAllowlistManager();

  void OnAuthPrefsChanged();

  void OnLoseForeground();

  // Must be destroyed after |local_state_|.
  std::unique_ptr<AwBrowserPolicyConnector> browser_policy_connector_;

  // If non-null, this object holds a pref store that will be taken by
  // AwBrowserProcess to create the |local_state_|.
  // The AwFeatureListCreator is owned by AwMainDelegate.
  raw_ptr<AwFeatureListCreator> aw_feature_list_creator_;

  std::unique_ptr<PrefService> local_state_;

  // Accessed on both UI and IO threads.
  scoped_refptr<AwSafeBrowsingUIManager> safe_browsing_ui_manager_;

  // Accessed on UI thread only.
  std::unique_ptr<safe_browsing::TriggerManager> safe_browsing_trigger_manager_;

  // These two are accessed on IO thread only.
  scoped_refptr<safe_browsing::RemoteSafeBrowsingDatabaseManager>
      safe_browsing_db_manager_;
  bool safe_browsing_db_manager_started_ = false;

  PrefChangeRegistrar pref_change_registrar_;

  // TODO(amalova): Consider to make AllowlistManager per-profile.
  // Accessed on UI and IO threads.
  std::unique_ptr<AwSafeBrowsingAllowlistManager>
      safe_browsing_allowlist_manager_;

  std::unique_ptr<VisibilityMetricsLogger> visibility_metrics_logger_;
  std::unique_ptr<AwContentsLifecycleNotifier> aw_contents_lifecycle_notifier_;
  std::unique_ptr<EnterpriseAuthenticationAppLinkManager> app_link_manager_;
  std::unique_ptr<embedder_support::OriginTrialsSettingsStorage>
      origin_trials_settings_storage_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
};

}  // namespace android_webview
#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PROCESS_H_
