// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_manager_impl.h"

#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/supports_user_data.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/on_device_translation/service_controller_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/on_device_translation/service/service_launcher.h"
#include "components/on_device_translation/service_controller.h"
#include "components/on_device_translation/service_controller_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace on_device_translation {
namespace {

constexpr char kCreateTranslatorErrorHistogram[] =
    "Translate.TranslatorApi.CreateTranslatorError";

class FakeServiceControllerManager : public ServiceControllerManager {
 public:
  FakeServiceControllerManager()
      : ServiceControllerManager(
            /*local_state=*/nullptr,
            base::BindRepeating(
                []() -> std::unique_ptr<OnDeviceTranslationServiceLauncher> {
                  return nullptr;
                })) {}
  ~FakeServiceControllerManager() override = default;

  void CreateTranslator(const url::Origin& origin,
                        const std::string& source_lang,
                        const std::string& target_lang,
                        OnDeviceTranslationController::CreateTranslatorCallback
                            callback) override {
    ++create_translator_call_count_;
    pending_callback_future_.SetValue(std::move(callback));
  }

  OnDeviceTranslationController::CreateTranslatorCallback
  TakePendingCallback() {
    return pending_callback_future_.Take();
  }

  int create_translator_call_count() const {
    return create_translator_call_count_;
  }

 private:
  int create_translator_call_count_ = 0;
  base::test::TestFuture<
      OnDeviceTranslationController::CreateTranslatorCallback>
      pending_callback_future_;
};

class TestCreateTranslatorClient
    : public blink::mojom::TranslationManagerCreateTranslatorClient {
 public:
  TestCreateTranslatorClient() = default;
  ~TestCreateTranslatorClient() override = default;

  mojo::PendingRemote<blink::mojom::TranslationManagerCreateTranslatorClient>
  BindNewPipeAndPassRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(disconnect_future_.GetCallback());
    return remote;
  }

  void ResetReceiver() { receiver_.reset(); }

  blink::mojom::CreateTranslatorResultPtr TakeResult() {
    return result_future_.Take();
  }

  bool WaitForDisconnect() { return disconnect_future_.Wait(); }

  bool WasCalled() const { return result_future_.IsReady(); }

  // blink::mojom::TranslationManagerCreateTranslatorClient:
  void OnResult(blink::mojom::CreateTranslatorResultPtr result,
                blink::mojom::TranslatorLanguageCodePtr source_lang,
                blink::mojom::TranslatorLanguageCodePtr target_lang,
                std::optional<uint64_t> input_quota) override {
    result_future_.SetValue(std::move(result));
  }

 private:
  mojo::Receiver<blink::mojom::TranslationManagerCreateTranslatorClient>
      receiver_{this};
  base::test::TestFuture<blink::mojom::CreateTranslatorResultPtr>
      result_future_;
  base::test::TestFuture<void> disconnect_future_;
};

class TestSupportsUserData : public base::SupportsUserData {
 public:
  TestSupportsUserData() = default;
  ~TestSupportsUserData() override = default;
};

blink::mojom::TranslatorCreateOptionsPtr MakeCreateOptions(
    const std::string& source_lang,
    const std::string& target_lang) {
  return blink::mojom::TranslatorCreateOptions::New(
      blink::mojom::TranslatorLanguageCode::New(source_lang),
      blink::mojom::TranslatorLanguageCode::New(target_lang),
      /*observer_remote=*/mojo::NullRemote());
}

class TranslationManagerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    controller_manager_ = static_cast<FakeServiceControllerManager*>(
        ServiceControllerManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_,
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<FakeServiceControllerManager>();
                })));
    process_host_ = std::make_unique<content::MockRenderProcessHost>(&profile_);
    context_user_data_ = std::make_unique<TestSupportsUserData>();
    TranslationManagerImpl::Bind(
        process_host_.get(), &profile_, context_user_data_.get(),
        url::Origin::Create(GURL("https://example.com")), &component_updater_,
        manager_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    manager_remote_.reset();
    context_user_data_.reset();
    controller_manager_ = nullptr;
    process_host_.reset();
  }

  base::test::ScopedFeatureList feature_list_{blink::features::kTranslationAPI};
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<content::MockRenderProcessHost> process_host_;
  testing::NiceMock<component_updater::MockComponentUpdateService>
      component_updater_;
  std::unique_ptr<TestSupportsUserData> context_user_data_;
  raw_ptr<FakeServiceControllerManager> controller_manager_ = nullptr;
  mojo::Remote<blink::mojom::TranslationManager> manager_remote_;
};

TEST_F(TranslationManagerImplTest, RecordsCreateTranslatorError) {
  using ControllerError = OnDeviceTranslationController::CreateTranslatorError;
  using BlinkError = blink::mojom::CreateTranslatorError;

  static constexpr auto kCases =
      std::to_array<std::pair<ControllerError, BlinkError>>({
          {ControllerError::kServiceCrashed, BlinkError::kServiceCrashed},
          {ControllerError::kLibraryInstallFailed,
           BlinkError::kLibraryInstallFailed},
          {ControllerError::kLanguagePackDownloadFailed,
           BlinkError::kLanguagePackDownloadFailed},
          {ControllerError::kNotEnoughDiskSpace,
           BlinkError::kNotEnoughDiskSpace},
          {ControllerError::kOffline, BlinkError::kOffline},
          {ControllerError::kUnsupportedDevice, BlinkError::kUnsupportedDevice},
          {ControllerError::kDataSaverEnabled, BlinkError::kDataSaverEnabled},
          {ControllerError::kExceedsLanguagePackCountLimitation,
           BlinkError::kExceedsLanguagePackCountLimitation},
          {ControllerError::kMeteredConnectionBlocked,
           BlinkError::kMeteredConnectionBlocked},
      });

  base::HistogramTester histogram_tester;
  for (const auto& [controller_error, expected_blink_error] : kCases) {
    TestCreateTranslatorClient client;
    manager_remote_->CreateTranslator(client.BindNewPipeAndPassRemote(),
                                      MakeCreateOptions("en", "ja"));
    OnDeviceTranslationController::CreateTranslatorCallback callback =
        controller_manager_->TakePendingCallback();
    std::move(callback).Run(base::unexpected(controller_error));

    blink::mojom::CreateTranslatorResultPtr result = client.TakeResult();
    ASSERT_TRUE(result->is_error());
    EXPECT_EQ(result->get_error(), expected_blink_error);
    histogram_tester.ExpectBucketCount(kCreateTranslatorErrorHistogram,
                                       expected_blink_error, 1);
  }
  histogram_tester.ExpectTotalCount(kCreateTranslatorErrorHistogram,
                                    std::size(kCases));
}

TEST_F(TranslationManagerImplTest, DoesNotRecordErrorWhenClientDisconnects) {
  base::HistogramTester histogram_tester;
  TestCreateTranslatorClient client;
  manager_remote_->CreateTranslator(client.BindNewPipeAndPassRemote(),
                                    MakeCreateOptions("en", "ja"));
  OnDeviceTranslationController::CreateTranslatorCallback callback =
      controller_manager_->TakePendingCallback();

  client.ResetReceiver();
  task_environment_.RunUntilIdle();

  std::move(callback).Run(
      base::unexpected(OnDeviceTranslationController::CreateTranslatorError::
                           kLibraryInstallFailed));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(client.WasCalled());
  histogram_tester.ExpectTotalCount(kCreateTranslatorErrorHistogram, 0);
}

TEST_F(TranslationManagerImplTest,
       DisconnectsClientWhenServiceManagerDropsCallback) {
  base::HistogramTester histogram_tester;
  TestCreateTranslatorClient client;
  manager_remote_->CreateTranslator(client.BindNewPipeAndPassRemote(),
                                    MakeCreateOptions("en", "ja"));
  OnDeviceTranslationController::CreateTranslatorCallback callback =
      controller_manager_->TakePendingCallback();

  callback.Reset();
  EXPECT_TRUE(client.WaitForDisconnect());
  EXPECT_FALSE(client.WasCalled());
  histogram_tester.ExpectTotalCount(kCreateTranslatorErrorHistogram, 0);
}

// Synchronous checks in `TranslationManagerImpl::CreateTranslator()` (such as
// the `TranslatorAPIAllowed` policy check) reject before calling
// `ServiceControllerManager::CreateTranslator()`, so `CreateTranslatorImpl()`
// never runs to record the histogram.
TEST_F(TranslationManagerImplTest, DoesNotRecordErrorOnEarlyPolicyRejection) {
  base::HistogramTester histogram_tester;
  profile_.GetPrefs()->SetBoolean(prefs::kTranslatorAPIAllowed, false);

  TestCreateTranslatorClient client;
  manager_remote_->CreateTranslator(client.BindNewPipeAndPassRemote(),
                                    MakeCreateOptions("en", "ja"));
  blink::mojom::CreateTranslatorResultPtr result = client.TakeResult();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            blink::mojom::CreateTranslatorError::kDisallowedByPolicy);
  EXPECT_EQ(controller_manager_->create_translator_call_count(), 0);
  histogram_tester.ExpectTotalCount(kCreateTranslatorErrorHistogram, 0);
}

TEST(TranslationManagerImplBindingTest, BindingNotAllowedByDefault) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithEmptyFeatureAndFieldTrialLists();
  EXPECT_FALSE(TranslationManagerImpl::IsBindingAllowed(
      /*requires_explicit_enable=*/true));
}

TEST(TranslationManagerImplBindingTest, BindingAllowedWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list(blink::features::kTranslationAPI);
  EXPECT_TRUE(TranslationManagerImpl::IsBindingAllowed(
      /*requires_explicit_enable=*/true));
}

TEST(TranslationManagerImplBindingTest, BindingNotAllowedWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(blink::features::kTranslationAPI);
  EXPECT_FALSE(TranslationManagerImpl::IsBindingAllowed(
      /*requires_explicit_enable=*/true));
}

TEST(TranslationManagerImplBindingTest,
     BindingNotAllowedWhenFeatureUsesDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithEmptyFeatureAndFieldTrialLists();

  auto feature_list = std::make_unique<base::FeatureList>();
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial("TestTrial", "Default");
  ASSERT_TRUE(trial);
  feature_list->RegisterFieldTrialOverride(
      blink::features::kTranslationAPI.name,
      base::FeatureList::OVERRIDE_USE_DEFAULT, trial);
  base::test::ScopedFeatureList trial_feature_list;
  trial_feature_list.InitWithFeatureList(std::move(feature_list));

  // Verify the gate rejects OVERRIDE_USE_DEFAULT even though the base feature
  // defaults to enabled and has a field-trial override entry.
  EXPECT_FALSE(TranslationManagerImpl::IsBindingAllowed(
      /*requires_explicit_enable=*/true));
  EXPECT_TRUE(base::FeatureList::IsEnabled(blink::features::kTranslationAPI));
  EXPECT_TRUE(base::FeatureList::GetInstance()->IsFeatureOverridden(
      blink::features::kTranslationAPI.name));
}

TEST(TranslationManagerImplBindingTest,
     BindingAllowedWithoutExplicitEnableRequirement) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithEmptyFeatureAndFieldTrialLists();
    EXPECT_TRUE(TranslationManagerImpl::IsBindingAllowed(
        /*requires_explicit_enable=*/false));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(blink::features::kTranslationAPI);
    EXPECT_TRUE(TranslationManagerImpl::IsBindingAllowed(
        /*requires_explicit_enable=*/false));
  }
}

}  // namespace
}  // namespace on_device_translation
