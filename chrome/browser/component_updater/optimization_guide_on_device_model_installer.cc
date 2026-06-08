// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "base/byte_count.h"
#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

namespace component_updater {
namespace {

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

// Extension id is ceofaddefefcbblgcgnibnonglccbfja.
constexpr char kOptimizationGuideModelsManifestName[] =
    "Optimization Guide On DeviceModels Manifest";
constexpr base::FilePath::CharType kManifestRelativeInstallDir[] =
    FILE_PATH_LITERAL("OptimizationGuideModelsManifest");
constexpr uint8_t kManifestPublicKeySHA256[32] = {
    0x24, 0xe5, 0x03, 0x34, 0x54, 0x52, 0x11, 0xb6, 0x26, 0xd8, 0x1d,
    0xed, 0x6b, 0x22, 0x15, 0x90, 0x9a, 0x44, 0xf0, 0x88, 0xdc, 0x19,
    0xfa, 0x5d, 0xd4, 0x55, 0xf7, 0x95, 0x88, 0xff, 0xfd, 0x8a};
static_assert(std::size(kManifestPublicKeySHA256) == crypto::kSHA256Length);

BASE_FEATURE(kModelManifestChannelFeature,
             "ModelManifestChannel",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kModelManifestChannel,
                   &kModelManifestChannelFeature,
                   "");

bool IsModelAlreadyInstalled(ComponentUpdateService* cus,
                             const std::string& extension_id,
                             const std::string& target_version = "") {
  CrxUpdateItem update_item;
  bool success = cus->GetComponentDetails(extension_id, &update_item);
  if (!success || !update_item.component.has_value() ||
      !update_item.component->version.IsValid()) {
    return false;
  }

  if (target_version.empty()) {
    return update_item.component->version.CompareToWildcardString("0.0.0.0") >
           0;
  }

  return update_item.component->version.CompareToWildcardString(
             target_version) == 0;
}

bool GetPublicKeyHashFromHex(const std::string& public_key_hex,
                             std::vector<uint8_t>* public_key_hash) {
  if (!base::HexStringToBytes(public_key_hex, public_key_hash)) {
    return false;
  }
  return public_key_hash->size() == 32;
}

base::FilePath GetComponentInstallDirectory() {
  base::FilePath local_install_path;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &local_install_path);
  return local_install_path;
}

void GetComponentFreeDiskSpace(
    const base::FilePath& path,
    base::OnceCallback<void(std::optional<base::ByteCount>)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(),
       optimization_guide::switches::
               ShouldGetFreeDiskSpaceWithUserVisiblePriorityTask()
           ? base::TaskPriority::USER_VISIBLE
           : base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](const base::FilePath& path) -> std::optional<base::ByteCount> {
            std::optional<int64_t> amount_of_free_disk_space =
                base::SysInfo::AmountOfFreeDiskSpace(path);
            if (!amount_of_free_disk_space) {
              return std::nullopt;
            }
            return base::ByteCount(*amount_of_free_disk_space);
          },
          path),
      std::move(callback));
}

// Legacy installer policy for the base and classifier models.
class OnDeviceModelInstallerPolicy
    : public OptimizationGuideOnDeviceModelInstallerPolicy {
 public:
  // `state_manager` has the lifetime till all profiles are closed. It could
  // slightly vary from lifetime of `this` which runs in separate task runner,
  // and could get destroyed slightly later than `state_manager`.
  explicit OnDeviceModelInstallerPolicy(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          state_manager)
      : state_manager_(state_manager) {}
  ~OnDeviceModelInstallerPolicy() override = default;

  void OnCustomUninstall() final {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&optimization_guide::OnDeviceModelComponentStateManager::
                           UninstallComplete,
                       state_manager_));
  }

  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const final {
    return optimization_guide::OnDeviceModelComponentStateManager::
        VerifyInstallation(install_dir, manifest);
  }

  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) final {
    if (state_manager_) {
      state_manager_->SetReady(version, install_dir, manifest);
    }
  }

 protected:
  // The on-device state manager should be accessed in the UI thread.
  base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
      state_manager_;
};

// Legacy Installer policy for the On-Device Base Model.
class OptimizationGuideOnDeviceBaseModelInstallerPolicy final
    : public OnDeviceModelInstallerPolicy {
 public:
  explicit OptimizationGuideOnDeviceBaseModelInstallerPolicy(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          state_manager,
      optimization_guide::OnDeviceModelRegistrationAttributes attributes)
      : OnDeviceModelInstallerPolicy(state_manager),
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

// Legacy Installer policy for the On-Device Classifier Model.
class OptimizationGuideOnDeviceClassifierModelInstallerPolicy final
    : public OnDeviceModelInstallerPolicy {
 public:
  explicit OptimizationGuideOnDeviceClassifierModelInstallerPolicy(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          state_manager)
      : OnDeviceModelInstallerPolicy(state_manager) {}
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

class OnDeviceModelComponentStateManagerDelegate final
    : public optimization_guide::OnDeviceModelComponentStateManager::Delegate {
 public:
  explicit OnDeviceModelComponentStateManagerDelegate(OnDeviceModelType type)
      : type_(type) {}
  ~OnDeviceModelComponentStateManagerDelegate() override = default;

  base::FilePath GetInstallDirectory() override {
    return GetComponentInstallDirectory();
  }

  void GetFreeDiskSpace(const base::FilePath& path,
                        base::OnceCallback<void(std::optional<base::ByteCount>)>
                            callback) override {
    GetComponentFreeDiskSpace(path, std::move(callback));
  }

  void RegisterInstaller(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          state_manager,
      optimization_guide::OnDeviceModelRegistrationAttributes attributes)
      override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!g_browser_process) {
      return;
    }
    ComponentUpdateService* cus = g_browser_process->component_updater();
    auto register_callback = base::BindOnce(
        [](base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
               state_manager,
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

  void Uninstall(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
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
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          state_manager,
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

  const OnDeviceModelType type_;
};

// A generic component installer policy for Manifest Component.
class ManifestComponentsInstallerPolicy final
    : public OptimizationGuideOnDeviceModelInstallerPolicy {
 public:
  // `asset_manager` has the lifetime till all profiles are closed. It could
  // slightly vary from lifetime of `this` which runs in separate task runner,
  // and could get destroyed slightly later than `state_manager`.
  ManifestComponentsInstallerPolicy(
      std::string public_key_hex,
      std::string target_version,
      std::string component_name,
      base::WeakPtr<optimization_guide::ManifestAssetManager> asset_manager)
      : public_key_hex_(std::move(public_key_hex)),
        target_version_(std::move(target_version)),
        component_name_(std::move(component_name)),
        asset_manager_(std::move(asset_manager)) {
    if (!GetPublicKeyHashFromHex(public_key_hex_, &public_key_hash_)) {
      LOG(ERROR) << "Invalid public key hex: [" << public_key_hex_ << "]";
    }
  }

  ~ManifestComponentsInstallerPolicy() override = default;

  ManifestComponentsInstallerPolicy(const ManifestComponentsInstallerPolicy&) =
      delete;
  ManifestComponentsInstallerPolicy& operator=(
      const ManifestComponentsInstallerPolicy&) = delete;

 private:
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override {
    return optimization_guide::ManifestAssetManager::VerifyInstallation(
        install_dir, manifest);
  }

  void OnCustomUninstall() override {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &optimization_guide::ManifestAssetManager::OnAssetUninstalled,
            asset_manager_, public_key_hex_));
  }

  base::FilePath GetRelativeInstallDir() const override {
    // Temporary redirection to avoid re-downloading the legacy model again.
    return std::ranges::equal(public_key_hash_, base::span(kPublicKeySHA256))
               ? base::FilePath(kInstallationRelativePath)
               : base::FilePath(FILE_PATH_LITERAL("OptGuideManifestModel"))
                     .AppendASCII(public_key_hex_);
  }

  void GetHash(std::vector<uint8_t>* hash) const override {
    *hash = public_key_hash_;
  }

  std::string GetName() const override {
    return base::StrCat(
        {"Optimization Guide Manifest Component: ", component_name_});
  }

  update_client::InstallerAttributes GetInstallerAttributes() const override {
    return {{"target_version", target_version_}};
  }

  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override {
    if (asset_manager_) {
      asset_manager_->OnAssetReady(public_key_hex_, version, install_dir);
    }
  }

  const std::string public_key_hex_;
  std::vector<uint8_t> public_key_hash_;
  const std::string target_version_;
  const std::string component_name_;
  // The manifest asset manager should be accessed in the UI thread.
  base::WeakPtr<optimization_guide::ManifestAssetManager> asset_manager_;
};

// Installer policy for the manifest component itself.
class ManifestMonitorInstallerPolicy final
    : public OptimizationGuideOnDeviceModelInstallerPolicy {
 public:
  explicit ManifestMonitorInstallerPolicy(
      base::RepeatingCallback<void(base::FilePath)> on_ready_callback)
      : on_ready_callback_(std::move(on_ready_callback)) {}

  ManifestMonitorInstallerPolicy(const ManifestMonitorInstallerPolicy&) =
      delete;
  ManifestMonitorInstallerPolicy& operator=(
      const ManifestMonitorInstallerPolicy&) = delete;

 private:
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override {
    return base::PathExists(
        install_dir.Append(optimization_guide::kManifestFileName));
  }

  base::FilePath GetRelativeInstallDir() const override {
    return base::FilePath(kManifestRelativeInstallDir);
  }

  void GetHash(std::vector<uint8_t>* hash) const override {
    hash->assign(std::begin(kManifestPublicKeySHA256),
                 std::end(kManifestPublicKeySHA256));
  }

  std::string GetName() const override {
    return kOptimizationGuideModelsManifestName;
  }

  update_client::InstallerAttributes GetInstallerAttributes() const override {
    return {{"manifest_channel", kModelManifestChannel.Get()}};
  }

  // Manifest should never be uninstalled.
  void OnCustomUninstall() override {}

  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(on_ready_callback_, install_dir));
  }

  base::RepeatingCallback<void(base::FilePath)> on_ready_callback_;
};

class ManifestAssetManagerDelegateImpl final
    : public optimization_guide::ManifestAssetManager::Delegate {
 public:
  base::CallbackListSubscription ListenForManifestReady(
      base::RepeatingCallback<void(base::FilePath)> on_ready) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    auto subscription = manifest_ready_callbacks_.Add(std::move(on_ready));
    if (!manifest_dir_.empty()) {
      manifest_ready_callbacks_.Notify(manifest_dir_);
    }
    MaybeRegisterManifestComponent();
    return subscription;
  }

  void GetFreeDiskSpace(base::OnceCallback<void(std::optional<base::ByteCount>)>
                            callback) const override {
    GetComponentFreeDiskSpace(GetComponentInstallDirectory(),
                              std::move(callback));
  }

  void RegisterOnDemandComponent(
      const std::string& public_key_hex,
      const std::string& target_version,
      const std::string& component_name,
      base::WeakPtr<optimization_guide::ManifestAssetManager> manager)
      override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!g_browser_process) {
      return;
    }

    ComponentUpdateService* cus = g_browser_process->component_updater();

    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<ManifestComponentsInstallerPolicy>(
            public_key_hex, target_version, component_name, manager));

    auto register_callback = base::BindOnce(
        [](base::WeakPtr<optimization_guide::ManifestAssetManager> manager,
           ComponentUpdateService* cus, const std::string& public_key_hex,
           const std::string& target_version) {
          std::vector<uint8_t> public_key_hash;
          std::string extension_id;
          if (GetPublicKeyHashFromHex(public_key_hex, &public_key_hash)) {
            extension_id =
                crx_file::id_util::GenerateIdFromHash(public_key_hash);
          }

          if (manager) {
            manager->InstallerRegistered(
                public_key_hex, target_version,
                IsModelAlreadyInstalled(cus, extension_id, target_version));
          }
        },
        manager, cus, public_key_hex, target_version);

    installer->Register(cus, std::move(register_callback));
  }

  void Uninstall(const std::string& public_key_hex,
                 base::WeakPtr<optimization_guide::ManifestAssetManager>
                     manager) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<ManifestComponentsInstallerPolicy>(
            public_key_hex, /*target_version=*/std::string(),
            /*component_name=*/std::string(), std::move(manager)))
        ->Uninstall();
  }

  void RequestUpdate(const std::string& public_key_hex,
                     bool is_background) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!g_browser_process) {
      return;
    }
    std::vector<uint8_t> public_key_hash;
    if (!GetPublicKeyHashFromHex(public_key_hex, &public_key_hash)) {
      LOG(ERROR) << "Invalid public key hex in RequestUpdate: ["
                 << public_key_hex << "]";
      return;
    }

    OptimizationGuideOnDeviceModelInstallerPolicy::UpdateOnDemand(
        crx_file::id_util::GenerateIdFromHash(public_key_hash),
        is_background ? OnDemandUpdater::Priority::BACKGROUND
                      : OnDemandUpdater::Priority::FOREGROUND);
  }

 private:
  void MaybeRegisterManifestComponent() {
    if (manifest_registered_) {
      return;
    }
    manifest_registered_ = true;
    if (!g_browser_process) {
      return;
    }

    ComponentUpdateService* cus = g_browser_process->component_updater();
    if (!cus) {
      return;
    }

    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<ManifestMonitorInstallerPolicy>(base::BindRepeating(
            &ManifestAssetManagerDelegateImpl::OnManifestReady,
            weak_ptr_factory_.GetWeakPtr())));
    installer->Register(
        cus, base::BindOnce([] {
          OptimizationGuideOnDeviceModelInstallerPolicy::UpdateOnDemand(
              crx_file::id_util::GenerateIdFromHash(kManifestPublicKeySHA256),
              OnDemandUpdater::Priority::FOREGROUND);
        }));
  }

  void OnManifestReady(base::FilePath install_dir) {
    manifest_dir_ = std::move(install_dir);
    manifest_ready_callbacks_.Notify(manifest_dir_);
  }

  base::FilePath manifest_dir_;
  bool manifest_registered_ = false;
  base::RepeatingCallbackList<void(base::FilePath)> manifest_ready_callbacks_;
  base::WeakPtrFactory<ManifestAssetManagerDelegateImpl> weak_ptr_factory_{
      this};
};

}  // namespace

bool OptimizationGuideOnDeviceModelInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::RequiresNetworkEncryption()
    const {
  return true;
}

update_client::CrxInstaller::Result
OptimizationGuideOnDeviceModelInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
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

std::unique_ptr<optimization_guide::ManifestAssetManager::Delegate>
CreateManifestAssetManagerDelegate() {
  return std::make_unique<ManifestAssetManagerDelegateImpl>();
}

}  // namespace component_updater
