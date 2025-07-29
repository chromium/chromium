// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/pref_names.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace optimization_guide {

namespace {

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
                        base::OnceCallback<void(int64_t)> callback) override {
    base::TaskTraits traits = {base::MayBlock(),
                               base::TaskPriority::BEST_EFFORT};
    if (optimization_guide::switches::
            ShouldGetFreeDiskSpaceWithUserVisiblePriorityTask()) {
      traits.UpdatePriority(base::TaskPriority::USER_VISIBLE);
    }

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, traits,
        base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, path),
        std::move(callback));
  }

  void RegisterInstaller(
      base::WeakPtr<OnDeviceModelComponentStateManager> state_manager,
      bool is_already_installing) override {
    if (!g_browser_process) {
      return;
    }
    component_updater::RegisterOptimizationGuideOnDeviceModelComponent(
        g_browser_process->component_updater(), std::move(state_manager),
        is_already_installing);
  }

  void Uninstall(base::WeakPtr<OnDeviceModelComponentStateManager>
                     state_manager) override {
    component_updater::UninstallOptimizationGuideOnDeviceModelComponent(
        std::move(state_manager));
  }
};

base::WeakPtr<OptimizationGuideGlobalState>& GetInstance() {
  static base::NoDestructor<base::WeakPtr<OptimizationGuideGlobalState>> instance;
  return *instance.get();
}

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
};

OptimizationGuideGlobalState::OptimizationGuideGlobalState()
    : model_broker_state_(
          g_browser_process->local_state(),
          std::make_unique<OnDeviceModelComponentStateManagerDelegate>(),
          base::BindRepeating(&LaunchService)) {
  model_broker_state_.Init();
  model_broker_state_.performance_classifier()
      .ListenForPerformanceClassAvailable(
          base::BindOnce(&ChromeOnDeviceModelServiceController::
                             RegisterPerformanceClassSyntheticTrial));
  model_broker_state_.performance_classifier().ScheduleEvaluation();
}
OptimizationGuideGlobalState::~OptimizationGuideGlobalState() = default;

scoped_refptr<OptimizationGuideGlobalState> OptimizationGuideGlobalState::CreateOrGet() {
  base::WeakPtr<OptimizationGuideGlobalState>& instance = GetInstance();
  if (!instance) {
    auto new_instance = base::WrapRefCounted(new OptimizationGuideGlobalState());
    instance = new_instance->weak_ptr_factory_.GetWeakPtr();
    return new_instance;
  }
  return scoped_refptr<OptimizationGuideGlobalState>(instance.get());
}

}  // namespace optimization_guide
