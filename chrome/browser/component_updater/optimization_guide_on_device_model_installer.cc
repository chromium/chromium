// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

namespace component_updater {
namespace {

using ::optimization_guide::OnDeviceModelComponentStateManager;

// Extension id is fklghjjljmnfjoepjmlobpekiapffcja.
constexpr char kManifestName[] = "Optimization Guide On Device Model";
constexpr base::FilePath::CharType kInstallationRelativePath[] =
    FILE_PATH_LITERAL("OptGuideOnDeviceModel");
constexpr uint8_t kPublicKeySHA256[32] = {
    0x5a, 0xb6, 0x79, 0x9b, 0x9c, 0xd5, 0x9e, 0x4f, 0x9c, 0xbe, 0x1f,
    0x4a, 0x80, 0xf5, 0x52, 0x90, 0x74, 0xea, 0x87, 0x3a, 0xf9, 0x91,
    0x00, 0x26, 0x43, 0x86, 0x03, 0x36, 0xa6, 0x38, 0x86, 0x63};
static_assert(std::size(kPublicKeySHA256) == crypto::kSHA256Length);

// Extension id is eidcjfoningnkhpoelgpjemmhmopkeoi.
constexpr char kClassifierModelManifestName[] =
    "Optimization Guide On Device Taxonomy Model";
constexpr base::FilePath::CharType kClassifierModelInstallationRelativePath[] =
    FILE_PATH_LITERAL("OptGuideOnDeviceClassifierModel");
constexpr uint8_t kClassifierModelPublicKeySHA256[32] = {
    0x48, 0x32, 0x95, 0xed, 0x8d, 0x6d, 0xa7, 0xfe, 0x4b, 0x6f, 0x94,
    0xcc, 0x7c, 0xef, 0xa4, 0xe8, 0xa3, 0x79, 0xd7, 0xe5, 0x79, 0x5f,
    0x53, 0x64, 0xef, 0xe7, 0x7b, 0xe1, 0x52, 0x44, 0x5b, 0x37};
static_assert(std::size(kClassifierModelPublicKeySHA256) ==
              crypto::kSHA256Length);

bool IsModelAlreadyInstalled(ComponentUpdateService* cus,
                             const std::string& extension_id) {
  CrxUpdateItem update_item;
  bool success = cus->GetComponentDetails(extension_id, &update_item);
  return success && update_item.component.has_value() &&
         update_item.component->version.IsValid() &&
         update_item.component->version.CompareToWildcardString("0.0.0.0") > 0;
}

// Installer policy for the On-Device Base Model.
class OptimizationGuideOnDeviceBaseModelInstallerPolicy
    : public OptimizationGuideOnDeviceModelInstallerPolicy {
 public:
  explicit OptimizationGuideOnDeviceBaseModelInstallerPolicy(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          state_manager,
      optimization_guide::OnDeviceModelRegistrationAttributes attributes)
      : OptimizationGuideOnDeviceModelInstallerPolicy(state_manager),
        attributes_(std::move(attributes)) {}
  ~OptimizationGuideOnDeviceBaseModelInstallerPolicy() override = default;

  base::FilePath GetRelativeInstallDir() const override {
    return base::FilePath(kInstallationRelativePath);
  }

  void GetHash(std::vector<uint8_t>* hash) const override {
    hash->assign(std::begin(kPublicKeySHA256), std::end(kPublicKeySHA256));
  }

  std::string GetName() const override { return kManifestName; }
  update_client::InstallerAttributes GetInstallerAttributes() const override {
    using Hint = optimization_guide::proto::OnDeviceModelPerformanceHint;
    return {
        {"cpu_support", attributes_.supported_hints.contains(
                            Hint::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU)
                            ? "yes"
                            : "no"},
        {"highest_quality_support",
         attributes_.supported_hints.contains(
             Hint::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY)
             ? "yes"
             : "no"},
        {"fastest_inference_support",
         attributes_.supported_hints.contains(
             Hint::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE)
             ? "yes"
             : "no"},
    };
  }

  static const std::string GetOnDeviceModelExtensionId() {
    return crx_file::id_util::GenerateIdFromHash(kPublicKeySHA256);
  }

 private:
  const optimization_guide::OnDeviceModelRegistrationAttributes attributes_;
};

// Installer policy for the On-Device Classifier Model.
class OptimizationGuideOnDeviceClassifierModelInstallerPolicy
    : public OptimizationGuideOnDeviceModelInstallerPolicy {
 public:
  explicit OptimizationGuideOnDeviceClassifierModelInstallerPolicy(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          state_manager)
      : OptimizationGuideOnDeviceModelInstallerPolicy(state_manager) {}
  ~OptimizationGuideOnDeviceClassifierModelInstallerPolicy() override = default;

  base::FilePath GetRelativeInstallDir() const override {
    return base::FilePath(kClassifierModelInstallationRelativePath);
  }

  void GetHash(std::vector<uint8_t>* hash) const override {
    hash->assign(std::begin(kClassifierModelPublicKeySHA256),
                 std::end(kClassifierModelPublicKeySHA256));
  }
  std::string GetName() const override { return kClassifierModelManifestName; }

  static const std::string GetExtensionId() {
    return crx_file::id_util::GenerateIdFromHash(
        kClassifierModelPublicKeySHA256);
  }
};

class OnDeviceModelComponentStateManagerDelegate
    : public OnDeviceModelComponentStateManager::Delegate {
 public:
  explicit OnDeviceModelComponentStateManagerDelegate(OnDeviceModelType type)
      : type_(type) {}
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
    // TODO(crbug.com/429140103): Convert base::SysInfo::AmountOfFreeDiskSpace
    // to return std::optional<base::ByteCount> and remove this wrapper.
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
        FROM_HERE,
        {base::MayBlock(),
         optimization_guide::switches::
                 ShouldGetFreeDiskSpaceWithUserVisiblePriorityTask()
             ? base::TaskPriority::USER_VISIBLE
             : base::TaskPriority::BEST_EFFORT},
        std::move(amount_of_free_disk_space_wrapper), std::move(callback));
  }

  void RegisterInstaller(
      base::WeakPtr<OnDeviceModelComponentStateManager> state_manager,
      optimization_guide::OnDeviceModelRegistrationAttributes attributes)
      override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!g_browser_process) {
      return;
    }
    ComponentUpdateService* cus = g_browser_process->component_updater();
    auto register_callback = base::BindOnce(
        [](base::WeakPtr<OnDeviceModelComponentStateManager> state_manager,
           ComponentUpdateService* cus, const std::string& extension_id) {
          if (state_manager) {
            state_manager->InstallerRegistered(
                IsModelAlreadyInstalled(cus, extension_id));
          }
        },
        state_manager, cus, GetComponentId());
    base::MakeRefCounted<ComponentInstaller>(
        CreateInstallerPolicy(state_manager, std::move(attributes)))
        ->Register(cus, std::move(register_callback));
  }

  void Uninstall(base::WeakPtr<OnDeviceModelComponentStateManager>
                     state_manager) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::MakeRefCounted<ComponentInstaller>(
        CreateInstallerPolicy(
            state_manager,
            optimization_guide::OnDeviceModelRegistrationAttributes({})))
        ->Uninstall();
  }

  void RequestUpdate(bool is_background) override {
    OnDemandUpdater::Priority priority = OnDemandUpdater::Priority::FOREGROUND;
    if (type_ == OnDeviceModelType::kBaseModel && is_background) {
      priority = OnDemandUpdater::Priority::BACKGROUND;
    }
    OptimizationGuideOnDeviceBaseModelInstallerPolicy::UpdateOnDemand(
        GetComponentId(), priority);
  }

  std::string GetComponentId() override {
    switch (type_) {
      case OnDeviceModelType::kClassifierModel:
        return OptimizationGuideOnDeviceClassifierModelInstallerPolicy::
            GetExtensionId();
      case OnDeviceModelType::kBaseModel:
        return OptimizationGuideOnDeviceBaseModelInstallerPolicy::
            GetOnDeviceModelExtensionId();
    }
  }

 private:
  std::unique_ptr<ComponentInstallerPolicy> CreateInstallerPolicy(
      base::WeakPtr<OnDeviceModelComponentStateManager> state_manager,
      optimization_guide::OnDeviceModelRegistrationAttributes attributes) {
    switch (type_) {
      case OnDeviceModelType::kClassifierModel:
        return std::make_unique<
            OptimizationGuideOnDeviceClassifierModelInstallerPolicy>(
            state_manager);
      case OnDeviceModelType::kBaseModel:
        return std::make_unique<
            OptimizationGuideOnDeviceBaseModelInstallerPolicy>(
            state_manager, std::move(attributes));
    }
  }

  OnDeviceModelType type_;
};

}  // namespace

OptimizationGuideOnDeviceModelInstallerPolicy::
    OptimizationGuideOnDeviceModelInstallerPolicy(
        base::WeakPtr<OnDeviceModelComponentStateManager> state_manager)
    : state_manager_(state_manager) {}

OptimizationGuideOnDeviceModelInstallerPolicy::
    ~OptimizationGuideOnDeviceModelInstallerPolicy() = default;

bool OptimizationGuideOnDeviceModelInstallerPolicy::VerifyInstallation(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) const {
  return OnDeviceModelComponentStateManager::VerifyInstallation(install_dir,
                                                                manifest);
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::RequiresNetworkEncryption()
    const {
  // This installer is only registered for users who use certain features, and
  // we do not want to expose that they are users of those features.
  return true;
}

update_client::CrxInstaller::Result
OptimizationGuideOnDeviceModelInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void OptimizationGuideOnDeviceModelInstallerPolicy::OnCustomUninstall() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceModelComponentStateManager::UninstallComplete,
                     state_manager_));
}

void OptimizationGuideOnDeviceModelInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  if (state_manager_) {
    state_manager_->SetReady(version, install_dir, manifest);
  }
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::AllowCachedCopies() const {
  return false;
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::
    AllowUpdatesOnMeteredConnections() const {
  return false;
}

update_client::InstallerAttributes
OptimizationGuideOnDeviceModelInstallerPolicy::GetInstallerAttributes() const {
  return {};
}

// static
void OptimizationGuideOnDeviceModelInstallerPolicy::UpdateOnDemand(
    const std::string& id,
    OnDemandUpdater::Priority priority) {
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      id, priority, base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS) {
          LOG(ERROR) << "Failed to update on-device model component with error "
                     << static_cast<int>(error);
        }
      }));
}

std::unique_ptr<
    optimization_guide::OnDeviceModelComponentStateManager::Delegate>
CreateOptimizationGuideOnDeviceModelComponentDelegate(OnDeviceModelType type) {
  return std::make_unique<OnDeviceModelComponentStateManagerDelegate>(type);
}

std::string GetOptimizationGuideOnDeviceModelExtensionId(
    OnDeviceModelType type) {
  switch (type) {
    case OnDeviceModelType::kClassifierModel:
      return OptimizationGuideOnDeviceClassifierModelInstallerPolicy::
          GetExtensionId();
    case OnDeviceModelType::kBaseModel:
      return OptimizationGuideOnDeviceBaseModelInstallerPolicy::
          GetOnDeviceModelExtensionId();
  }
}

}  // namespace component_updater
