// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/optimization_guide/prediction/chrome_profile_download_service_tracker.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/installer_policies/prediction_model_component_installer.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/delivery/prediction_manager.h"
#include "components/optimization_guide/core/delivery/prediction_model_store.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/override_manifest_asset_manager_delegate.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/prefs/pref_service.h"
#include "components/services/unzip/content/unzip_service.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

BASE_FEATURE(kOptimizationGuideManifestBroker,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
BASE_FEATURE(kOnDeviceAiDefaultEnabled, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

namespace {

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
bool HasInstalledOnDeviceModel(const PrefService& local_state) {
  for (const auto [_, value] : local_state.GetDict(
           model_execution::prefs::localstate::kManifestAssetLedger)) {
    const base::DictValue* entry_dict = value.GetIfDict();
    if (!entry_dict) {
      continue;
    }
    const std::string* requested_version =
        entry_dict->FindString("requested_version");
    if (requested_version && !requested_version->empty() &&
        *requested_version != "uninstalling") {
      return true;
    }
  }
  return local_state.GetTime(
             model_execution::prefs::localstate::
                 kLastTimeEligibleForOnDeviceModelDownload) !=
         base::Time::Min();
}

void LaunchService(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
        pending_receiver) {
  content::ServiceProcessHost::Launch<
      on_device_model::mojom::OnDeviceModelService>(
      std::move(pending_receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName("On-Device Model Service")
          .Pass());
}

std::unique_ptr<ManifestAssetManagerDelegate> CreateManifestDelegate() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kOptimizationGuideManifestOverrideSwitch)) {
    base::FilePath override_path = command_line->GetSwitchValuePath(
        kOptimizationGuideManifestOverrideSwitch);
    return std::make_unique<OverrideManifestAssetManagerDelegate>(
        override_path);
  }
  return component_updater::CreateManifestAssetManagerDelegate();
}

#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

base::WeakPtr<OptimizationGuideGlobalState>& GetInstance() {
  static base::NoDestructor<base::WeakPtr<OptimizationGuideGlobalState>>
      instance;
  return *instance.get();
}

base::FilePath GetBaseStoreDir() {
  base::FilePath model_downloads_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &model_downloads_dir);
  model_downloads_dir = model_downloads_dir.Append(
      optimization_guide::kOptimizationGuideModelStoreDirPrefix);
  return model_downloads_dir;
}

}  // namespace

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
bool ShouldEnableOnDeviceAiByDefault(const PrefService& local_state,
                                     version_info::Channel channel,
                                     bool is_official_branded_build) {
  const bool is_official_stable =
      is_official_branded_build && channel == version_info::Channel::STABLE;
  // Automated test suites (such as `chrome_ai_wpt_tests`) and benchmarks pass
  // `--optimization-guide-manifest-override` on fresh user-data-dirs to inject
  // a test model manifest without toggling `chrome://settings/ai`.
  const bool has_manifest_override =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          kOptimizationGuideManifestOverrideSwitch);
  return is_official_stable || has_manifest_override ||
         base::FeatureList::IsEnabled(kOnDeviceAiDefaultEnabled) ||
         HasInstalledOnDeviceModel(local_state);
}

bool ShouldEnableOnDeviceAiByDefault(const PrefService& local_state,
                                     version_info::Channel channel) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const bool is_official_branded_build = version_info::IsOfficialBuild();
#else
  const bool is_official_branded_build = false;
#endif
  return ShouldEnableOnDeviceAiByDefault(local_state, channel,
                                         is_official_branded_build);
}
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

class ChromeOnDeviceModelServiceController final {
 public:
  static void RegisterPerformanceClassSyntheticTrial() {
    auto perf_class = optimization_guide::PerformanceClassFromPref(
        *g_browser_process->local_state());
    if (perf_class != OnDeviceModelPerformanceClass::kUnknown) {
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          "SyntheticOnDeviceModelPerformanceClass",
          SyntheticTrialGroupForPerformanceClass(perf_class),
          variations::SyntheticTrialAnnotationMode::kCurrentLog);
    }
  }
};

ChromePredictionManager::ChromePredictionManager()
    : prediction_model_store_(*g_browser_process->local_state()),
      prediction_manager_(&prediction_model_store_,
                          g_browser_process->shared_url_loader_factory(),
                          g_browser_process->local_state(),
                          g_browser_process->GetApplicationLocale(),
                          OptimizationGuideLogger::GetInstance(),
                          base::BindRepeating(&unzip::LaunchUnzipper)) {
  prediction_model_store_.Initialize(GetBaseStoreDir());
  prediction_manager_.MaybeInitializeModelDownloads(
      profile_download_service_tracker_, g_browser_process->local_state());
}
ChromePredictionManager::~ChromePredictionManager() = default;

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
OptimizationGuideGlobalState::OptimizationGuideGlobalState(
    LaunchServiceCallback launch_service_callback) {
  auto manifest_broker_state = std::make_unique<ManifestBrokerState>(
      *g_browser_process->local_state(), CreateManifestDelegate(),
      launch_service_callback, g_browser_process->component_updater());

  manifest_broker_state->performance_classifier()
      .ListenForPerformanceClassAvailable(
          base::BindOnce(&ChromeOnDeviceModelServiceController::
                             RegisterPerformanceClassSyntheticTrial));
  manifest_broker_state->performance_classifier().ScheduleEvaluation();

  on_device_capability_ = std::move(manifest_broker_state);
}
#else  // !BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
OptimizationGuideGlobalState::OptimizationGuideGlobalState() {

#if BUILDFLAG(IS_ANDROID)
  on_device_capability_ = std::make_unique<ModelBrokerAndroid>(
      *g_browser_process->local_state(), model_provider());
#else   // !BUILDFLAG(IS_ANDROID)
  // Create a stub capability that can't do anything.
  on_device_capability_ = std::make_unique<OnDeviceCapability>();
#endif  // BUILDFLAG(IS_ANDROID)
}
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

void RegisterPredictionModelComponent(
    proto::OptimizationTarget target,
    base::WeakPtr<PredictionModelComponentUpdateListener> listener) {
  auto* cus = g_browser_process->component_updater();
  if (!cus) {
    return;
  }
  component_updater::RegisterPredictionModelComponent(cus, target, listener);
}

OptimizationGuideGlobalState::~OptimizationGuideGlobalState() = default;

scoped_refptr<OptimizationGuideGlobalState>
OptimizationGuideGlobalState::CreateOrGet() {
  base::WeakPtr<OptimizationGuideGlobalState>& instance = GetInstance();
  if (!instance) {
#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
    auto new_instance = base::WrapRefCounted(
        new OptimizationGuideGlobalState(base::BindRepeating(&LaunchService)));
#else
    auto new_instance =
        base::WrapRefCounted(new OptimizationGuideGlobalState());
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
    instance = new_instance->weak_ptr_factory_.GetWeakPtr();
    return new_instance;
  }
  return scoped_refptr<OptimizationGuideGlobalState>(instance.get());
}

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
// static
scoped_refptr<OptimizationGuideGlobalState>
OptimizationGuideGlobalState::CreateForTesting() {
  return base::WrapRefCounted(
      new OptimizationGuideGlobalState(base::DoNothing()));
}
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

OptimizationGuideGlobalFeature::OptimizationGuideGlobalFeature() {
#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
  if (g_browser_process && g_browser_process->local_state()) {
    PrefService& local_state = *g_browser_process->local_state();
    local_state.SetDefaultPrefValue(
        model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
        base::Value(
            ShouldEnableOnDeviceAiByDefault(local_state, chrome::GetChannel())));
  }
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
}

OptimizationGuideGlobalFeature::~OptimizationGuideGlobalFeature() = default;

OptimizationGuideGlobalState& OptimizationGuideGlobalFeature::Get() {
  if (!global_state_) {
    global_state_ = OptimizationGuideGlobalState::CreateOrGet();
  }
  return *global_state_;
}

OptimizationGuideModelProvider&
OptimizationGuideGlobalFeature::GetModelProvider() {
  return Get().model_provider();
}

}  // namespace optimization_guide
