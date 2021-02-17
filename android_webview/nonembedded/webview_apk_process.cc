// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/webview_apk_process.h"

#include "android_webview/common/aw_paths.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/base_paths_android.h"
#include "base/path_service.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/update_client/update_client.h"

namespace android_webview {

// static
WebViewApkProcess* WebViewApkProcess::GetInstance() {
  // TODO(crbug.com/1179303): Add check to assert this is only loaded by
  // LibraryProcessType PROCESS_WEBVIEW_NONEMBEDDED.
  static base::NoDestructor<WebViewApkProcess> instance;
  return instance.get();
}

WebViewApkProcess::WebViewApkProcess() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "WebViewApkProcess");

  RegisterPathProvider();
  CreatePrefService();
}

WebViewApkProcess::~WebViewApkProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

PrefService* WebViewApkProcess::GetPrefService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_.get();
}

void WebViewApkProcess::CreatePrefService() {
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  PrefServiceFactory pref_service_factory;

  RegisterPrefs(pref_registry.get());

  base::FilePath app_data_dir;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &app_data_dir);
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(app_data_dir.Append(
          FILE_PATH_LITERAL("WebView Nonembedded Preferences"))));
  pref_service_ = pref_service_factory.Create(pref_registry);
}

void WebViewApkProcess::RegisterPrefs(PrefRegistrySimple* pref_registry) {
  update_client::RegisterPrefs(pref_registry);
}

}  // namespace android_webview
