// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/webview_apk_process.h"

#include "android_webview/common/aw_paths.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/base_paths_android.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/update_client/update_client.h"

namespace android_webview {

namespace {

static WebViewApkProcess* g_webview_apk_process = nullptr;

}  // namespace

// static
WebViewApkProcess* WebViewApkProcess::GetInstance() {
  DCHECK(g_webview_apk_process);
  return g_webview_apk_process;
}

// static
// Must be called exactly once during the process startup.
void WebViewApkProcess::Init() {
  // TODO(crbug.com/40749658): Add check to assert this is only loaded by
  // LibraryProcessType PROCESS_WEBVIEW_NONEMBEDDED.

  // This doesn't have to be thread safe, because it should only happen once on
  // the main thread before any GetInstances calls are made.
  DCHECK(!g_webview_apk_process);
  g_webview_apk_process = new WebViewApkProcess();
}

WebViewApkProcess::WebViewApkProcess() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "WebViewApkProcess");

  // There's no UI message pump in nonembedded WebView, using
  // `base::MessagePumpType::JAVA` so that the `SingleThreadExecutor` will bind
  // to the java thread the `WebViewApkProcess` is created on.
  main_task_executor_ = std::make_unique<base::SingleThreadTaskExecutor>(
      base::MessagePumpType::JAVA);

  RegisterPathProvider();
  component_updater::RegisterPathProvider(
      /*components_system_root_key=*/android_webview::DIR_COMPONENTS_ROOT,
      /*components_system_root_key_alt=*/android_webview::DIR_COMPONENTS_ROOT,
      /*components_user_root_key=*/android_webview::DIR_COMPONENTS_ROOT);

  CreatePrefService();
}

WebViewApkProcess::~WebViewApkProcess() = default;

PrefService* WebViewApkProcess::GetPrefService() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return pref_service_.get();
}

void WebViewApkProcess::CreatePrefService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  update_client::RegisterPrefs(pref_registry);
}

}  // namespace android_webview
