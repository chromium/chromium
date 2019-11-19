// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_process.h"

#include "android_webview/browser/aw_browser_context.h"
#include "base/base_paths_posix.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace android_webview {

namespace prefs {

// String that specifies the Android account type to use for Negotiate
// authentication.
const char kAuthAndroidNegotiateAccountType[] =
    "auth.android_negotiate_account_type";

// Whitelist containing servers for which Integrated Authentication is enabled.
const char kAuthServerWhitelist[] = "auth.server_whitelist";

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
}

AwBrowserProcess::~AwBrowserProcess() {
  g_aw_browser_process = nullptr;
}

void AwBrowserProcess::PreMainMessageLoopRun() {
  pref_change_registrar_.Init(local_state());
  auto auth_pref_callback = base::BindRepeating(
      &AwBrowserProcess::OnAuthPrefsChanged, base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAuthServerWhitelist, auth_pref_callback);
  pref_change_registrar_.Add(prefs::kAuthAndroidNegotiateAccountType,
                             auth_pref_callback);

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

AwBrowserPolicyConnector* AwBrowserProcess::browser_policy_connector() {
  if (!browser_policy_connector_)
    CreateBrowserPolicyConnector();
  return browser_policy_connector_.get();
}

void AwBrowserProcess::CreateBrowserPolicyConnector() {
  DCHECK(!browser_policy_connector_);

  browser_policy_connector_ =
      aw_feature_list_creator_->TakeBrowserPolicyConnector();
  DCHECK(browser_policy_connector_);
}

void AwBrowserProcess::InitSafeBrowsing() {
  CreateSafeBrowsingUIManager();
  CreateSafeBrowsingWhitelistManager();
}

void AwBrowserProcess::CreateSafeBrowsingUIManager() {
  safe_browsing_ui_manager_ = new AwSafeBrowsingUIManager();
}

void AwBrowserProcess::CreateSafeBrowsingWhitelistManager() {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                       base::TaskPriority::BEST_EFFORT});
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      base::CreateSingleThreadTaskRunner({BrowserThread::IO});
  safe_browsing_whitelist_manager_ =
      std::make_unique<AwSafeBrowsingWhitelistManager>(background_task_runner,
                                                       io_task_runner);
}

safe_browsing::RemoteSafeBrowsingDatabaseManager*
AwBrowserProcess::GetSafeBrowsingDBManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!safe_browsing_db_manager_) {
    safe_browsing_db_manager_ =
        new safe_browsing::RemoteSafeBrowsingDatabaseManager();
  }

  if (!safe_browsing_db_manager_started_) {
    // V4ProtocolConfig is not used. Just create one with empty values..
    safe_browsing::V4ProtocolConfig config("", false, "", "");
    safe_browsing_db_manager_->StartOnIOThread(
        GetSafeBrowsingUIManager()->GetURLLoaderFactoryOnIOThread(), config);
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
            /*referrer_chain_provider=*/nullptr,
            /*local_state_prefs=*/nullptr);
  }

  return safe_browsing_trigger_manager_.get();
}

AwSafeBrowsingWhitelistManager*
AwBrowserProcess::GetSafeBrowsingWhitelistManager() const {
  return safe_browsing_whitelist_manager_.get();
}

AwSafeBrowsingUIManager* AwBrowserProcess::GetSafeBrowsingUIManager() const {
  return safe_browsing_ui_manager_.get();
}

// static
void AwBrowserProcess::RegisterNetworkContextLocalStatePrefs(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterStringPref(prefs::kAuthServerWhitelist, std::string());
  pref_registry->RegisterStringPref(prefs::kAuthAndroidNegotiateAccountType,
                                    std::string());
}

network::mojom::HttpAuthDynamicParamsPtr
AwBrowserProcess::CreateHttpAuthDynamicParams() {
  network::mojom::HttpAuthDynamicParamsPtr auth_dynamic_params =
      network::mojom::HttpAuthDynamicParams::New();

  auth_dynamic_params->server_allowlist =
      local_state()->GetString(prefs::kAuthServerWhitelist);
  auth_dynamic_params->android_negotiate_account_type =
      local_state()->GetString(prefs::kAuthAndroidNegotiateAccountType);

  auth_dynamic_params->ntlm_v2_enabled = true;

  return auth_dynamic_params;
}

void AwBrowserProcess::OnAuthPrefsChanged() {
  content::GetNetworkService()->ConfigureHttpAuthPrefs(
      CreateHttpAuthDynamicParams());
}

}  // namespace android_webview
