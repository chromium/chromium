// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/ai_embeddings_component_installer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"
#include "components/component_updater/component_installer.h"
#include "components/passage_embeddings/core/passage_embeddings_service_controller.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {
namespace {

class AIEmbeddingsComponentInstallerTest : public testing::Test {
 public:
  AIEmbeddingsComponentInstallerTest() {
    policy_ = GetAIEmbeddingsComponentInstallerPolicyForTesting();
  }

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {
    AISemanticEmbedderServiceLauncher::Get()
        ->controller()
        ->MaybeUpdateModelInfo(std::nullopt);
  }

  base::FilePath GetInstallDir() const { return temp_dir_.GetPath(); }

  void CreateValidPackage() {
    base::FilePath model_path = GetInstallDir().AppendASCII("model.tflite");
    ASSERT_TRUE(base::WriteFile(model_path, "model data"));
    base::FilePath sp_model_path = GetInstallDir().AppendASCII("tokenizer.spm");
    ASSERT_TRUE(base::WriteFile(sp_model_path, "sp data"));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ComponentInstallerPolicy> policy_;
};

TEST_F(AIEmbeddingsComponentInstallerTest, VerifyInstallation) {
  base::DictValue manifest;

  // Initially, neither file exists.
  EXPECT_FALSE(policy_->VerifyInstallation(manifest, GetInstallDir()));

  CreateValidPackage();
  EXPECT_TRUE(policy_->VerifyInstallation(manifest, GetInstallDir()));

  base::FilePath model_path = GetInstallDir().AppendASCII("model.tflite");
  base::DeleteFile(model_path);
  EXPECT_FALSE(policy_->VerifyInstallation(manifest, GetInstallDir()));

  CreateValidPackage();
  base::FilePath sp_model_path = GetInstallDir().AppendASCII("tokenizer.spm");
  base::DeleteFile(sp_model_path);
  EXPECT_FALSE(policy_->VerifyInstallation(manifest, GetInstallDir()));
}

TEST_F(AIEmbeddingsComponentInstallerTest, ComponentReady) {
  base::DictValue manifest;
  CreateValidPackage();
  base::Version version("1.2.3.4");
  policy_->ComponentReady(version, GetInstallDir(), std::move(manifest));

  auto* controller = AISemanticEmbedderServiceLauncher::Get()->controller();
  EXPECT_TRUE(controller->IsModelAvailable());
  EXPECT_EQ(controller->embeddings_model_path(),
            GetInstallDir().AppendASCII("model.tflite"));
  EXPECT_EQ(controller->sp_model_path(),
            GetInstallDir().AppendASCII("tokenizer.spm"));
}

TEST_F(AIEmbeddingsComponentInstallerTest, PolicyAttributes) {
  EXPECT_TRUE(policy_->SupportsGroupPolicyEnabledComponentUpdates());
  EXPECT_FALSE(policy_->RequiresNetworkEncryption());
  EXPECT_EQ(policy_->GetName(), "AI Embeddings Model");
  EXPECT_EQ(policy_->GetRelativeInstallDir(),
            base::FilePath(FILE_PATH_LITERAL("AIEmbeddings")));

  std::vector<uint8_t> hash;
  policy_->GetHash(&hash);
  EXPECT_EQ(hash.size(), 32u);

  EXPECT_TRUE(policy_->GetInstallerAttributes().empty());

  base::DictValue manifest;
  EXPECT_EQ(policy_->OnCustomInstall(manifest, GetInstallDir()).result.code, 0);
  policy_->OnCustomUninstall();
}

}  // namespace
}  // namespace component_updater
