// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/optimization_guide/prediction/chrome_profile_download_service_tracker.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/pref_names.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/delivery/prediction_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
#include "components/services/unzip/content/unzip_service.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace optimization_guide {

namespace {

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
class OnDeviceModelComponentStateManagerDelegate
    : public OnDeviceModelComponentStateManager::Delegate {
 public:
  ~OnDeviceModelComponentStateManagerDelegate() override = default;

  base::FilePath GetInstallDirectory() override {
    base::FilePath local_install_path;
    base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                           &local_install_path);
    return local_install_path;
  }

  void GetFreeDiskSpace(const base::FilePath& path,
                        base::OnceCallback<void(std::optional<base::ByteCount>)>
                            callback) override {
    base::TaskTraits traits = {base::MayBlock(),
                               base::TaskPriority::BEST_EFFORT};
    if (optimization_guide::switches::
            ShouldGetFreeDiskSpaceWithUserVisiblePriorityTask()) {
      traits.UpdatePriority(base::TaskPriority::USER_VISIBLE);
    }

    // TODO(https://crbug.com/429140103): Convert
    // base::SysInfo::AmountOfFreeDiskSpace to return
    // std::optional<base::ByteCount> and remove this wrapper.
    auto amount_of_free_disk_space_wrapper = base::BindOnce(
        [](const base::FilePath& path) -> std::optional<base::ByteCount> {
          std::optional<int64_t> amount_of_free_disk_space =
              base::SysInfo::AmountOfFreeDiskSpace(path);
          if (!amount_of_free_disk_space) {
            return std::nullopt;
          }
          return base::ByteCount(*amount_of_free_disk_space);
        },
        path);

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, traits, std::move(amount_of_free_disk_space_wrapper),
        std::move(callback));
  }

  void RegisterInstaller(
      base::WeakPtr<OnDeviceModelComponentStateManager> state_manager,
      OnDeviceModelRegistrationAttributes attributes) override {
    if (!g_browser_process) {
      return;
    }
    component_updater::RegisterOptimizationGuideOnDeviceModelComponent(
        g_browser_process->component_updater(), std::move(state_manager),
        std::move(attributes));
  }

  void Uninstall(base::WeakPtr<OnDeviceModelComponentStateManager>
                     state_manager) override {
    component_updater::UninstallOptimizationGuideOnDeviceModelComponent(
        std::move(state_manager));
  }
};

void LaunchService(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
        pending_receiver) {
  CHECK(features::CanLaunchOnDeviceModelService());
  content::ServiceProcessHost::Launch<
      on_device_model::mojom::OnDeviceModelService>(
      std::move(pending_receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName("On-Device Model Service")
          .Pass());
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

  static void RegisterPerformanceHintSyntheticTrial(
      proto::OnDeviceModelPerformanceHint performance_hint) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "SyntheticOnDeviceModelPerformanceHint",
        SyntheticTrialGroupForPerformanceHint(performance_hint),
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
  }
};

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
// Registers a field trial once the model is ready.
class ChromeModelComponentStateManagerObserver final
    : public OnDeviceModelComponentStateManager::Observer {
 public:
  explicit ChromeModelComponentStateManagerObserver(
      base::WeakPtr<OnDeviceModelComponentStateManager> state_manager) {
    if (!state_manager) {
      return;
    }
    observation_.Observe(state_manager.get());
    if (const OnDeviceModelComponentState* state = state_manager->GetState()) {
      StateChanged(state);
    }
  }

  // OnDeviceModelComponentStateManager::Observer:
  void StateChanged(const OnDeviceModelComponentState* state) override {
    if (state) {
      ChromeOnDeviceModelServiceController::
          RegisterPerformanceHintSyntheticTrial(
              state->GetBaseModelSpec().selected_performance_hint);
    }
  }

 private:
  base::ScopedObservation<OnDeviceModelComponentStateManager,
                          OnDeviceModelComponentStateManager::Observer>
      observation_{this};
};
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

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

OptimizationGuideGlobalState::OptimizationGuideGlobalState()
    : on_device_capability_(
#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
          *g_browser_process->local_state(),
          prediction_manager_.prediction_manager(),
          std::make_unique<OnDeviceModelComponentStateManagerDelegate>(),
          base::BindRepeating(&LaunchService)
#elif BUILDFLAG(IS_ANDROID)
          *g_browser_process->local_state(),
          prediction_manager_.prediction_manager()
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
      ) {
#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
  component_state_manager_observer_ =
      std::make_unique<ChromeModelComponentStateManagerObserver>(
          on_device_capability_.component_state_manager().GetWeakPtr());
  on_device_capability_.performance_classifier()
      .ListenForPerformanceClassAvailable(
          base::BindOnce(&ChromeOnDeviceModelServiceController::
                             RegisterPerformanceClassSyntheticTrial));
  on_device_capability_.performance_classifier().ScheduleEvaluation();
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
}
OptimizationGuideGlobalState::~OptimizationGuideGlobalState() = default;

scoped_refptr<OptimizationGuideGlobalState>
OptimizationGuideGlobalState::CreateOrGet() {
  base::WeakPtr<OptimizationGuideGlobalState>& instance = GetInstance();
  if (!instance) {
    auto new_instance =
        base::WrapRefCounted(new OptimizationGuideGlobalState());
    instance = new_instance->weak_ptr_factory_.GetWeakPtr();
    return new_instance;
  }
  return scoped_refptr<OptimizationGuideGlobalState>(instance.get());
}

OptimizationGuideGlobalFeature::OptimizationGuideGlobalFeature() = default;

OptimizationGuideGlobalFeature::~OptimizationGuideGlobalFeature() = default;

OptimizationGuideGlobalState& OptimizationGuideGlobalFeature::Get() {
  if (!global_state_) {
    global_state_ = OptimizationGuideGlobalState::CreateOrGet();
  }
  return *global_state_;
}

OptimizationGuideModelProvider&
OptimizationGuideGlobalFeature::GetModelProvider() {
  return Get().prediction_manager();
}

}  // namespace optimization_guide
