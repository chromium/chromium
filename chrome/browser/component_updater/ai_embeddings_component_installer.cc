// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/ai_embeddings_component_installer.h"

#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"
#include "components/passage_embeddings/core/passage_embeddings_service_controller.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {
// CRX ID: ddkjpondgmdhgaiodldnoebnfcjbckih
// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
constexpr std::array<uint8_t, 32> kAIEmbeddingsPublicKeySHA256 = {
    0x33, 0xa9, 0xfe, 0xd3, 0x6c, 0x37, 0x60, 0x8e, 0x3b, 0x3d, 0xe4,
    0x1d, 0x52, 0x91, 0x2a, 0x87, 0xdd, 0x95, 0x72, 0xaa, 0xaa, 0xd8,
    0xe9, 0x71, 0xdd, 0x66, 0x5f, 0x88, 0x6c, 0x08, 0xae, 0x33};

constexpr char kAIEmbeddingsManifestName[] = "AI Embeddings Model";
constexpr base::FilePath::CharType kModelFileName[] =
    FILE_PATH_LITERAL("model.tflite");
constexpr base::FilePath::CharType kSpModelFileName[] =
    FILE_PATH_LITERAL("tokenizer.spm");

base::FilePath GetInstalledModelPath(const base::FilePath& base) {
  return base.Append(kModelFileName);
}

base::FilePath GetInstalledSpModelPath(const base::FilePath& base) {
  return base.Append(kSpModelFileName);
}

// Policy for the AI Embeddings Component Installer.
// Responsible for installing and updating the AI Embeddings model files.
class AIEmbeddingsComponentInstallerPolicy
    : public component_updater::ComponentInstallerPolicy {
 public:
  AIEmbeddingsComponentInstallerPolicy() = default;
  ~AIEmbeddingsComponentInstallerPolicy() override = default;

 private:
  // ComponentInstallerPolicy overrides:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override {
    return true;
  }

  bool RequiresNetworkEncryption() const override { return false; }

  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) override {
    return update_client::CrxInstaller::Result(
        update_client::InstallError::NONE);
  }

  void OnCustomUninstall() override {}

  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override {
    return base::PathExists(GetInstalledModelPath(install_dir)) &&
           base::PathExists(GetInstalledSpModelPath(install_dir));
  }

  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override {
    optimization_guide::proto::PassageEmbeddingsModelMetadata metadata;
    metadata.set_input_window_size(2048);
    metadata.set_output_size(768);

    std::optional<optimization_guide::proto::Any> any_metadata =
        optimization_guide::AnyWrapProto(metadata);

    uint64_t version_num = 0;
    for (uint32_t component : version.components()) {
      version_num = (version_num << 16) + component;
    }

    optimization_guide::ModelInfo model_info{
        .model_file_path = GetInstalledModelPath(install_dir),
        .additional_files = {GetInstalledSpModelPath(install_dir)},
        .version = static_cast<int64_t>(version_num),
        .model_metadata = any_metadata,
    };

    AISemanticEmbedderServiceLauncher::Get()
        ->controller()
        ->MaybeUpdateModelInfo(&model_info);
  }

  base::FilePath GetRelativeInstallDir() const override {
    return base::FilePath(FILE_PATH_LITERAL("AIEmbeddings"));
  }

  void GetHash(std::vector<uint8_t>* hash) const override {
    hash->assign(kAIEmbeddingsPublicKeySHA256.begin(),
                 kAIEmbeddingsPublicKeySHA256.end());
  }

  std::string GetName() const override { return kAIEmbeddingsManifestName; }

  update_client::InstallerAttributes GetInstallerAttributes() const override {
    return update_client::InstallerAttributes();
  }

  bool AllowCachedCopies() const override { return false; }
};
}  // namespace

namespace component_updater {
std::unique_ptr<ComponentInstallerPolicy>
GetAIEmbeddingsComponentInstallerPolicyForTesting() {
  return std::make_unique<AIEmbeddingsComponentInstallerPolicy>();
}

void RegisterAIEmbeddingsComponent(ComponentUpdateService* cus,
                                   PrefService* local_state) {
  CHECK(local_state);
  if (!base::FeatureList::IsEnabled(blink::features::kAIEmbeddingsAPI) &&
      !base::FeatureList::IsEnabled(
          blink::features::kAIEmbeddingsAPIForWorkers)) {
    return;
  }

  if (optimization_guide::
          GetGenAILocalFoundationalModelEnterprisePolicySettings(local_state) ==
      optimization_guide::model_execution::prefs::
          GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed) {
    return;
  }

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<AIEmbeddingsComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

void DeleteAIEmbeddingsComponent(const base::FilePath& user_data_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(base::IgnoreResult(&base::DeletePathRecursively),
                     user_data_dir.Append(FILE_PATH_LITERAL("AIEmbeddings"))));
}

}  // namespace component_updater
