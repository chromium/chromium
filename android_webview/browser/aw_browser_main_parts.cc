// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_main_parts.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_policy_connector.h"
#include "android_webview/browser/aw_browser_terminator.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_metrics_service_client.h"
#include "android_webview/browser/net/aw_network_change_notifier_factory.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_paths.h"
#include "android_webview/common/aw_resource.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/crash_reporter/aw_crash_reporter_client.h"
#include "base/android/apk_assets.h"
#include "base/android/build_info.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/base_paths_android.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/crash_dump_manager_android.h"
#include "components/heap_profiling/supervisor.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service_factory.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "services/preferences/tracked/segregated_pref_store.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/gl/gl_surface.h"

namespace {

// These prefs go in the JsonPrefStore, and will persist across runs. Other
// prefs go in the InMemoryPrefStore, and will be lost when the process ends.
const char* const kPersistentPrefsWhitelist[] = {
    // Random seed value for variation's entropy providers, used to assign
    // experiment groups.
    metrics::prefs::kMetricsLowEntropySource,
    // Used by CachingPermutedEntropyProvider to cache generated values.
    variations::prefs::kVariationsPermutedEntropyCache,
};

// Shows notifications which correspond to PersistentPrefStore's reading errors.
void HandleReadError(PersistentPrefStore::PrefReadError error) {}

base::FilePath GetPrefStorePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path);
  path = path.Append(FILE_PATH_LITERAL("pref_store"));
  return path;
}

std::unique_ptr<PrefService> CreatePrefService(
    policy::BrowserPolicyConnectorBase* browser_policy_connector) {
  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
  // We only use the autocomplete feature of Autofill, which is controlled via
  // the manager_delegate. We don't use the rest of Autofill, which is why it is
  // hardcoded as disabled here.
  // TODO(crbug.com/873740): The following also disables autocomplete.
  // Investigate what the intended behavior is.
  pref_registry->RegisterBooleanPref(autofill::prefs::kAutofillProfileEnabled,
                                     false);
  pref_registry->RegisterBooleanPref(
      autofill::prefs::kAutofillCreditCardEnabled, false);
  policy::URLBlacklistManager::RegisterProfilePrefs(pref_registry.get());

  pref_registry->RegisterStringPref(
      android_webview::prefs::kWebRestrictionsAuthority, std::string());

  android_webview::AwURLRequestContextGetter::RegisterPrefs(
      pref_registry.get());
  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  variations::VariationsService::RegisterPrefs(pref_registry.get());
  safe_browsing::RegisterProfilePrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;

  std::set<std::string> persistent_prefs;
  for (const char* const pref_name : kPersistentPrefsWhitelist)
    persistent_prefs.insert(pref_name);

  // SegregatedPrefStore may be validated with a MAC (message authentication
  // code). On Android, the store is protected by app sandboxing, so validation
  // is unnnecessary. Thus validation_delegate is null.
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<SegregatedPrefStore>(
          base::MakeRefCounted<InMemoryPrefStore>(),
          base::MakeRefCounted<JsonPrefStore>(GetPrefStorePath()),
          persistent_prefs, /*validation_delegate=*/nullptr));
  pref_service_factory.set_managed_prefs(
      base::MakeRefCounted<policy::ConfigurationPolicyPrefStore>(
          browser_policy_connector,
          browser_policy_connector->GetPolicyService(),
          browser_policy_connector->GetHandlerList(),
          policy::POLICY_LEVEL_MANDATORY));
  pref_service_factory.set_read_error_callback(
      base::BindRepeating(&HandleReadError));

  return pref_service_factory.Create(pref_registry);
}

}  // namespace

namespace android_webview {

AwBrowserMainParts::AwBrowserMainParts(AwContentBrowserClient* browser_client)
    : browser_client_(browser_client) {
}

AwBrowserMainParts::~AwBrowserMainParts() {
}

int AwBrowserMainParts::PreEarlyInitialization() {
  // Network change notifier factory must be singleton, only set factory
  // instance while it is not been created.
  // In most cases, this check is not necessary because SetFactory should be
  // called only once, but both webview and native cronet calls this function,
  // in case of building both webview and cronet to one app, it is required to
  // avoid crashing the app.
  if (!net::NetworkChangeNotifier::GetFactory()) {
    net::NetworkChangeNotifier::SetFactory(
        new AwNetworkChangeNotifierFactory());
  }

  // Creates a MessageLoop for Android WebView if doesn't yet exist.
  DCHECK(!main_message_loop_.get());
  if (!base::MessageLoopCurrent::IsSet())
    main_message_loop_.reset(new base::MessageLoopForUI);
  return service_manager::RESULT_CODE_NORMAL_EXIT;
}

int AwBrowserMainParts::PreCreateThreads() {
  base::android::MemoryPressureListenerAndroid::Initialize(
      base::android::AttachCurrentThread());
  ::crash_reporter::ChildExitObserver::Create();

  // We need to create the safe browsing specific directory even if the
  // AwSafeBrowsingConfigHelper::GetSafeBrowsingEnabled() is false
  // initially, because safe browsing can be enabled later at runtime
  // on a per-webview basis.
  base::FilePath safe_browsing_dir;
  if (base::PathService::Get(android_webview::DIR_SAFE_BROWSING,
                             &safe_browsing_dir)) {
    if (!base::PathExists(safe_browsing_dir))
      base::CreateDirectory(safe_browsing_dir);
  }

  base::FilePath crash_dir;
  if (crash_reporter::IsCrashReporterEnabled()) {
    if (base::PathService::Get(android_webview::DIR_CRASH_DUMPS, &crash_dir)) {
      if (!base::PathExists(crash_dir))
        base::CreateDirectory(crash_dir);
    }
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewSandboxedRenderer)) {
    // Create the renderers crash manager on the UI thread.
    ::crash_reporter::ChildExitObserver::GetInstance()->RegisterClient(
        std::make_unique<AwBrowserTerminator>(crash_dir));
  }

  browser_policy_connector_ = std::make_unique<AwBrowserPolicyConnector>();
  pref_service_ = CreatePrefService(browser_policy_connector_.get());
  AwMetricsServiceClient::GetInstance()->Initialize(pref_service_.get());
  aw_field_trial_creator_.SetUpFieldTrials(pref_service_.get());

  return service_manager::RESULT_CODE_NORMAL_EXIT;
}

void AwBrowserMainParts::PreMainMessageLoopRun() {
  DCHECK(pref_service_);
  DCHECK(browser_policy_connector_);
  AwBrowserContext* context = browser_client_->InitBrowserContext(
      std::move(pref_service_), std::move(browser_policy_connector_));
  context->PreMainMessageLoopRun(browser_client_->GetNetLog());

  content::RenderFrameHost::AllowInjectingJavaScriptForAndroidWebView();
}

bool AwBrowserMainParts::MainMessageLoopRun(int* result_code) {
  // Android WebView does not use default MessageLoop. It has its own
  // Android specific MessageLoop.
  return true;
}

void AwBrowserMainParts::ServiceManagerConnectionStarted(
    content::ServiceManagerConnection* connection) {
  heap_profiling::Mode mode = heap_profiling::GetModeForStartup();
  if (mode != heap_profiling::Mode::kNone) {
    heap_profiling::Supervisor::GetInstance()->Start(connection,
                                                     base::OnceClosure());
  }
}

}  // namespace android_webview
