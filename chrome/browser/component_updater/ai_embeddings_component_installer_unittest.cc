// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/ai_embeddings_component_installer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/passage_embeddings/core/passage_embeddings_service_controller.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {
namespace {

class AIEmbeddingsComponentInstallerTest : public testing::Test {
 public:
  AIEmbeddingsComponentInstallerTest() {
    policy_ = GetAIEmbeddingsComponentInstallerPolicyForTesting();
    optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(
        pref_service_.registry());
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
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  testing::NiceMock<MockComponentUpdateService> mock_cus_;

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

TEST_F(AIEmbeddingsComponentInstallerTest, RegistersWhenUnset) {
  // Do not set the preference, simulating an unmanaged device.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_cus_, RegisterComponent(testing::_))
      .WillOnce([&](const component_updater::ComponentRegistration&) {
        run_loop.Quit();
        return true;
      });
  RegisterAIEmbeddingsComponent(&mock_cus_, &pref_service_);
  run_loop.Run();
}

TEST_F(AIEmbeddingsComponentInstallerTest, RegistersWhenAllowed) {
  pref_service_.SetInteger(
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(
          optimization_guide::model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_cus_, RegisterComponent(testing::_))
      .WillOnce([&](const component_updater::ComponentRegistration&) {
        run_loop.Quit();
        return true;
      });
  RegisterAIEmbeddingsComponent(&mock_cus_, &pref_service_);
  run_loop.Run();
}

TEST_F(AIEmbeddingsComponentInstallerTest, DoesNotRegisterWhenDisallowed) {
  pref_service_.SetInteger(
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           GenAILocalFoundationalModelEnterprisePolicySettings::
                               kDisallowed));

  EXPECT_CALL(mock_cus_, RegisterComponent(testing::_)).Times(0);
  RegisterAIEmbeddingsComponent(&mock_cus_, &pref_service_);
}

TEST_F(AIEmbeddingsComponentInstallerTest, DeleteComponent) {
  base::FilePath component_dir =
      GetInstallDir().Append(FILE_PATH_LITERAL("AIEmbeddings"));
  ASSERT_TRUE(base::CreateDirectory(component_dir));
  ASSERT_TRUE(base::WriteFile(component_dir.AppendASCII("test_file"), "data"));

  EXPECT_TRUE(base::PathExists(component_dir));

  DeleteAIEmbeddingsComponent(GetInstallDir());
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_FALSE(base::PathExists(component_dir));
}

}  // namespace
}  // namespace component_updater
