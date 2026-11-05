// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/ai_embeddings_component_installer.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/passage_embeddings/core/passage_embeddings_service_controller.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace component_updater {
namespace {

using testing::_;

class AIEmbeddingsComponentInstallerTest : public testing::Test {
 public:
  AIEmbeddingsComponentInstallerTest() {
    feature_list_.InitAndEnableFeature(blink::features::kAIEmbeddingsAPI);
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
  // Uninstalling deletes files under DIR_COMPONENT_USER, which unit tests
  // otherwise resolve to the real user data directory. Declared before
  // `task_environment_` so it outlives the pending deletions that run when the
  // task environment is destroyed.
  base::ScopedPathOverride component_user_dir_override_{DIR_COMPONENT_USER};
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  testing::NiceMock<MockComponentUpdateService> mock_cus_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ComponentInstallerPolicy> policy_;
  base::test::ScopedFeatureList feature_list_;
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
  // The model is only loaded while the "On-device AI" setting allows it.
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      optimization_guide::model_execution::prefs::localstate::
          kOnDeviceAiUserSettingsEnabled,
      true);
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

// TestingBrowserProcess has no component updater on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(AIEmbeddingsComponentInstallerTest,
       ComponentReadyWhenDisallowedUnregisters) {
  auto cus = std::make_unique<testing::NiceMock<MockComponentUpdateService>>();
  EXPECT_CALL(*cus, UnregisterComponent(GetAIEmbeddingsComponentId()))
      .WillOnce(testing::Return(true));
  TestingBrowserProcess::GetGlobal()->SetComponentUpdater(std::move(cus));
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetBoolean(optimization_guide::model_execution::prefs::
                              localstate::kEmbeddingApiModelDownloadEligible,
                          true);
  // The setting was turned off while the component was being installed.
  local_state->SetBoolean(optimization_guide::model_execution::prefs::
                              localstate::kOnDeviceAiUserSettingsEnabled,
                          false);

  CreateValidPackage();
  policy_->ComponentReady(base::Version("1.2.3.4"), GetInstallDir(),
                          base::DictValue());
  // ComponentReady() posts ManageAIEmbeddingsComponentRegistration(), which
  // clears the pref and unregisters the component.
  ASSERT_TRUE(base::test::RunUntil([&] {
    return !local_state->GetBoolean(
        optimization_guide::model_execution::prefs::localstate::
            kEmbeddingApiModelDownloadEligible);
  }));

  auto* controller = AISemanticEmbedderServiceLauncher::Get()->controller();
  EXPECT_FALSE(controller->IsModelAvailable());
  TestingBrowserProcess::GetGlobal()->SetComponentUpdater(nullptr);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(AIEmbeddingsComponentInstallerTest,
       ComponentReadyWithoutLocalStateDoesNotLoadModel) {
  // Local state goes away with the browser process at shutdown, while a
  // ComponentReady() posted after an install may still run.
  auto* controller = AISemanticEmbedderServiceLauncher::Get()->controller();
  ASSERT_FALSE(controller->IsModelAvailable());
  TestingBrowserProcess::DeleteInstance();
  CreateValidPackage();
  policy_->ComponentReady(base::Version("1.2.3.4"), GetInstallDir(),
                          base::DictValue());

  EXPECT_FALSE(controller->IsModelAvailable());
  TestingBrowserProcess::CreateInstance();
}

TEST_F(AIEmbeddingsComponentInstallerTest, UninstallDropsLoadedModel) {
  // An update finished after the setting was turned off and back on, so
  // ComponentReady() loaded it. The unregister that was deferred until the end
  // of the update then deletes its files.
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      optimization_guide::model_execution::prefs::localstate::
          kOnDeviceAiUserSettingsEnabled,
      true);
  CreateValidPackage();
  policy_->ComponentReady(base::Version("1.2.3.4"), GetInstallDir(),
                          base::DictValue());
  auto* controller = AISemanticEmbedderServiceLauncher::Get()->controller();
  ASSERT_TRUE(controller->IsModelAvailable());

  base::MakeRefCounted<ComponentInstaller>(
      GetAIEmbeddingsComponentInstallerPolicyForTesting())
      ->Uninstall();
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !controller->IsModelAvailable(); }));
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

TEST_F(AIEmbeddingsComponentInstallerTest,
       DoesNotRegisterWhenFeaturesDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({},
                                 {blink::features::kAIEmbeddingsAPI,
                                  blink::features::kAIEmbeddingsAPIForWorkers,
                                  blink::features::kAIDecisionModelAPI});

  EXPECT_CALL(mock_cus_, RegisterComponent(testing::_)).Times(0);
  RegisterAIEmbeddingsComponent(&mock_cus_, &pref_service_);
}

TEST_F(AIEmbeddingsComponentInstallerTest,
       RegistersWhenDecisionModelFeatureEnabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({blink::features::kAIDecisionModelAPI},
                                 {blink::features::kAIEmbeddingsAPI,
                                  blink::features::kAIEmbeddingsAPIForWorkers});

  base::RunLoop run_loop;
  EXPECT_CALL(mock_cus_, RegisterComponent(testing::_))
      .WillOnce([&](const component_updater::ComponentRegistration&) {
        run_loop.Quit();
        return true;
      });
  RegisterAIEmbeddingsComponent(&mock_cus_, &pref_service_);
  run_loop.Run();
}

TEST_F(AIEmbeddingsComponentInstallerTest, RegistersWhenWorkerFeatureEnabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({blink::features::kAIEmbeddingsAPIForWorkers},
                                 {blink::features::kAIEmbeddingsAPI,
                                  blink::features::kAIDecisionModelAPI});

  base::RunLoop run_loop;
  EXPECT_CALL(mock_cus_, RegisterComponent(testing::_))
      .WillOnce([&](const component_updater::ComponentRegistration&) {
        run_loop.Quit();
        return true;
      });
  RegisterAIEmbeddingsComponent(&mock_cus_, &pref_service_);
  run_loop.Run();
}

TEST_F(AIEmbeddingsComponentInstallerTest, RegistersWhenBothFeaturesEnabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({blink::features::kAIEmbeddingsAPI,
                                  blink::features::kAIEmbeddingsAPIForWorkers},
                                 {});

  base::RunLoop run_loop;
  EXPECT_CALL(mock_cus_, RegisterComponent(testing::_))
      .WillOnce([&](const component_updater::ComponentRegistration&) {
        run_loop.Quit();
        return true;
      });
  RegisterAIEmbeddingsComponent(&mock_cus_, &pref_service_);
  run_loop.Run();
}

TEST_F(AIEmbeddingsComponentInstallerTest,
       ManageRegistrationResumesWhenEligible) {
  // Set the pref to true, simulating that the download is eligible to resume.
  pref_service_.SetBoolean(optimization_guide::model_execution::prefs::
                               localstate::kEmbeddingApiModelDownloadEligible,
                           true);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_cus_, RegisterComponent(_))
      .WillOnce([&](const component_updater::ComponentRegistration&) {
        run_loop.Quit();
        return true;
      });

  ManageAIEmbeddingsComponentRegistration(&mock_cus_, &pref_service_);
  run_loop.Run();
}

TEST_F(AIEmbeddingsComponentInstallerTest,
       ManageRegistrationDoesNotResumeWhenIneligible) {
  // Ensure the pref is false (default).
  EXPECT_FALSE(pref_service_.GetBoolean(
      optimization_guide::model_execution::prefs::localstate::
          kEmbeddingApiModelDownloadEligible));

  // Should not be registered.
  EXPECT_CALL(mock_cus_, RegisterComponent(_)).Times(0);
  ManageAIEmbeddingsComponentRegistration(&mock_cus_, &pref_service_);
}

TEST_F(AIEmbeddingsComponentInstallerTest, IsModelAllowed) {
  EXPECT_TRUE(IsAIEmbeddingsModelAllowed(&pref_service_));

  pref_service_.SetBoolean(optimization_guide::model_execution::prefs::
                               localstate::kOnDeviceAiUserSettingsEnabled,
                           false);
  EXPECT_FALSE(IsAIEmbeddingsModelAllowed(&pref_service_));

  pref_service_.SetBoolean(optimization_guide::model_execution::prefs::
                               localstate::kOnDeviceAiUserSettingsEnabled,
                           true);
  pref_service_.SetInteger(
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           GenAILocalFoundationalModelEnterprisePolicySettings::
                               kDisallowed));
  EXPECT_FALSE(IsAIEmbeddingsModelAllowed(&pref_service_));
}

TEST_F(AIEmbeddingsComponentInstallerTest,
       ManageRegistrationUnregistersWhenUserSettingDisabled) {
  pref_service_.SetBoolean(optimization_guide::model_execution::prefs::
                               localstate::kEmbeddingApiModelDownloadEligible,
                           true);
  pref_service_.SetBoolean(optimization_guide::model_execution::prefs::
                               localstate::kOnDeviceAiUserSettingsEnabled,
                           false);

  EXPECT_CALL(mock_cus_, RegisterComponent(_)).Times(0);
  EXPECT_CALL(mock_cus_, UnregisterComponent(GetAIEmbeddingsComponentId()))
      .WillOnce(testing::Return(true));
  ManageAIEmbeddingsComponentRegistration(&mock_cus_, &pref_service_);

  EXPECT_FALSE(pref_service_.GetBoolean(
      optimization_guide::model_execution::prefs::localstate::
          kEmbeddingApiModelDownloadEligible));
}

TEST_F(AIEmbeddingsComponentInstallerTest,
       ManageRegistrationUninstallsWhenDisallowedAndNotRegistered) {
  pref_service_.SetBoolean(optimization_guide::model_execution::prefs::
                               localstate::kEmbeddingApiModelDownloadEligible,
                           true);
  pref_service_.SetInteger(
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           GenAILocalFoundationalModelEnterprisePolicySettings::
                               kDisallowed));

  // A version installed in an earlier session.
  const base::FilePath install_root =
      base::PathService::CheckedGet(DIR_COMPONENT_USER)
          .Append(policy_->GetRelativeInstallDir());
  const base::FilePath installed_version = install_root.AppendASCII("1.2.3.4");
  ASSERT_TRUE(base::CreateDirectory(installed_version));
  ASSERT_TRUE(base::PathExists(installed_version));

  // Not registered in this session: falls back to deleting installed files.
  EXPECT_CALL(mock_cus_, RegisterComponent(_)).Times(0);
  EXPECT_CALL(mock_cus_, UnregisterComponent(GetAIEmbeddingsComponentId()))
      .WillOnce(testing::Return(false));
  ManageAIEmbeddingsComponentRegistration(&mock_cus_, &pref_service_);

  EXPECT_FALSE(pref_service_.GetBoolean(
      optimization_guide::model_execution::prefs::localstate::
          kEmbeddingApiModelDownloadEligible));
  // The uninstall deletes the version directories on a ThreadPool sequence,
  // then the emptied install directory; OnCustomUninstall() then posts to the
  // UI thread, which wakes RunUntil().
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !base::PathExists(install_root); }));
}

TEST_F(AIEmbeddingsComponentInstallerTest,
       ManageRegistrationWatchesSettingWhenFeaturesDisabled) {
  // A model was downloaded in an earlier session, and the APIs that use it are
  // off in this one, so the component isn't registered and ComponentReady()
  // never runs.
  feature_list_.Reset();
  feature_list_.InitWithFeatures({},
                                 {blink::features::kAIEmbeddingsAPI,
                                  blink::features::kAIEmbeddingsAPIForWorkers,
                                  blink::features::kAIDecisionModelAPI});
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetBoolean(optimization_guide::model_execution::prefs::
                              localstate::kOnDeviceAiUserSettingsEnabled,
                          true);
  local_state->SetBoolean(optimization_guide::model_execution::prefs::
                              localstate::kEmbeddingApiModelDownloadEligible,
                          true);
  const base::FilePath install_root =
      base::PathService::CheckedGet(DIR_COMPONENT_USER)
          .Append(policy_->GetRelativeInstallDir());
  ASSERT_TRUE(base::CreateDirectory(install_root.AppendASCII("1.2.3.4")));

  EXPECT_CALL(mock_cus_, RegisterComponent(_)).Times(0);
  ManageAIEmbeddingsComponentRegistration(&mock_cus_, local_state);

  // Turning off "On-device AI" later in the session deletes the model.
  local_state->SetBoolean(optimization_guide::model_execution::prefs::
                              localstate::kOnDeviceAiUserSettingsEnabled,
                          false);
  ASSERT_FALSE(local_state->GetBoolean(
      optimization_guide::model_execution::prefs::localstate::
          kEmbeddingApiModelDownloadEligible));
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !base::PathExists(install_root); }));
}

}  // namespace
}  // namespace component_updater
