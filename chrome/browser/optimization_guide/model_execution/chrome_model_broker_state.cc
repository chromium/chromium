// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/chrome_model_broker_state.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/optimization_guide/model_execution/chrome_on_device_model_service_controller.h"
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
      scoped_refptr<OnDeviceModelComponentStateManager> state_manager,
      bool is_already_installing) override {
    if (!g_browser_process) {
      return;
    }
    component_updater::RegisterOptimizationGuideOnDeviceModelComponent(
        g_browser_process->component_updater(), state_manager->GetWeakPtr(),
        is_already_installing);
  }

  void Uninstall(scoped_refptr<OnDeviceModelComponentStateManager>
                     state_manager) override {
    component_updater::UninstallOptimizationGuideOnDeviceModelComponent(
        state_manager->GetWeakPtr());
  }
};

base::WeakPtr<ChromeModelBrokerState>& GetInstance() {
  static base::NoDestructor<base::WeakPtr<ChromeModelBrokerState>> instance;
  return *instance.get();
}

}  // namespace

ChromeModelBrokerState::ChromeModelBrokerState() {
  component_state_manager_ = OnDeviceModelComponentStateManager::CreateOrGet(
      g_browser_process->local_state(),
      std::make_unique<OnDeviceModelComponentStateManagerDelegate>());
  component_state_manager_->OnStartup();

  service_controller_ =
      base::MakeRefCounted<ChromeOnDeviceModelServiceController>(
          component_state_manager_->GetWeakPtr());
  service_controller_->Init();

  if ((base::FeatureList::IsEnabled(
           optimization_guide::features::kLogOnDeviceMetricsOnStartup) ||
       optimization_guide::features::IsOnDeviceExecutionEnabled()) &&
      component_state_manager_->NeedsPerformanceClassUpdate()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &OnDeviceModelServiceController::EnsurePerformanceClassAvailable,
            service_controller_->GetWeakPtr(), base::DoNothing()),
        optimization_guide::features::GetOnDeviceStartupMetricDelay());
  }
  // If the perf class was previously determined, register that.
  service_controller_->RegisterPerformanceClassSyntheticTrial(
      optimization_guide::PerformanceClassFromPref(
          *g_browser_process->local_state()));
}
ChromeModelBrokerState::~ChromeModelBrokerState() = default;

scoped_refptr<ChromeModelBrokerState> ChromeModelBrokerState::CreateOrGet() {
  base::WeakPtr<ChromeModelBrokerState>& instance = GetInstance();
  if (!instance) {
    auto new_instance = base::WrapRefCounted(new ChromeModelBrokerState());
    instance = new_instance->weak_ptr_factory_.GetWeakPtr();
    return new_instance;
  }
  return scoped_refptr<ChromeModelBrokerState>(instance.get());
}

std::unique_ptr<OnDeviceAssetManager>
ChromeModelBrokerState::CreateAssetManager(
    OptimizationGuideModelProvider* provider) {
  return std::make_unique<OnDeviceAssetManager>(
      g_browser_process->local_state(), service_controller_->GetWeakPtr(),
      component_state_manager_->GetWeakPtr(), provider);
}

}  // namespace optimization_guide
