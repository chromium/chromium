// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_mediator.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/editor_geolocation_mock_provider.h"
#include "chrome/browser/ash/input_method/editor_geolocation_provider.h"
#include "chrome/browser/ash/input_method/editor_service_connector.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/feedback/feedback_constants.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/ime_bridge.h"

namespace ash::input_method {
namespace {

using ::testing::Eq;
using ::testing::Return;

// An in-process Orca service, so that a test can act as the service.
class MockOrcaService : public orca::mojom::OrcaService {
 public:
  MockOrcaService() = default;
  ~MockOrcaService() override = default;

  void Bind(mojo::PendingReceiver<orca::mojom::OrcaService> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // orca::mojom::OrcaService:
  MOCK_METHOD(
      void,
      BindEditor,
      (mojo::PendingAssociatedRemote<orca::mojom::SystemActuator>,
       mojo::PendingAssociatedRemote<orca::mojom::TextQueryProvider>,
       mojo::PendingAssociatedReceiver<orca::mojom::EditorClientConnector>,
       mojo::PendingAssociatedReceiver<orca::mojom::EditorEventSink>,
       orca::mojom::EditorConfigPtr),
      (override));

 private:
  mojo::Receiver<orca::mojom::OrcaService> receiver_{this};
};

class EditorMediatorTest : public ChromeAshTestBase {
 public:
  EditorMediatorTest() = default;
  ~EditorMediatorTest() override = default;

  TestingProfile& profile() { return profile_; }

 private:
  base::test::ScopedFeatureList feature_list_{chromeos::features::kOrcaDogfood};
  TestingProfile profile_;
};

TEST_F(EditorMediatorTest,
       SurroundingTextChangedDoesNotChangeSelectedTextLength) {
  std::unique_ptr<EditorGeolocationProvider> geolocation_provider =
      std::make_unique<EditorGeolocationMockProvider>("us");
  EditorMediator mediator(TestingBrowserProcess::GetGlobal()
                              ->GetFeatures()
                              ->application_locale_storage(),
                          &profile(),
                          manta::MantaServiceFactory::GetForProfile(&profile()),
                          std::move(geolocation_provider));

  IMEBridge::Get()->SetCurrentInputContext(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  mediator.FetchAndUpdateInputContextForTesting();

  mediator.OnSurroundingTextChanged(u"a", gfx::Range(0, 1));

  EXPECT_EQ(mediator.GetEditorOpportunityMode(), EditorOpportunityMode::kWrite);
}

TEST_F(EditorMediatorTest, CacheContextChangesSelectedTextLength) {
  std::unique_ptr<EditorGeolocationProvider> geolocation_provider =
      std::make_unique<EditorGeolocationMockProvider>("us");
  EditorMediator mediator(TestingBrowserProcess::GetGlobal()
                              ->GetFeatures()
                              ->application_locale_storage(),
                          &profile(),
                          manta::MantaServiceFactory::GetForProfile(&profile()),
                          std::move(geolocation_provider));

  IMEBridge::Get()->SetCurrentInputContext(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  mediator.FetchAndUpdateInputContextForTesting();
  mediator.OnSurroundingTextChanged(u"a", gfx::Range(0, 1));

  mediator.CacheContext();

  EXPECT_EQ(mediator.GetEditorOpportunityMode(),
            EditorOpportunityMode::kRewrite);
}

// Uses an in-process Orca service, because unit tests cannot launch the
// service process.
class EditorMediatorServiceConnectionTest : public EditorMediatorTest {
 public:
  EditorMediatorServiceConnectionTest() = default;
  ~EditorMediatorServiceConnectionTest() override = default;

 protected:
  // EditorMediatorTest:
  void SetUpInitParams(AshTestHelper::InitParams& init_params) override {
    EditorMediatorTest::SetUpInitParams(init_params);
    auto shell_delegate = std::make_unique<TestShellDelegate>();
    shell_delegate->SetSendSpecializedFeatureFeedbackCallback(
        send_feedback_callback_.Get());
    init_params.delegate = std::move(shell_delegate);
  }

  MockOrcaService orca_service_;
  base::MockCallback<TestShellDelegate::SendSpecializedFeatureFeedbackCallback>
      send_feedback_callback_;

 private:
  EditorServiceConnector::LaunchCallback launch_callback_ =
      base::BindRepeating(&MockOrcaService::Bind,
                          base::Unretained(&orca_service_));
  base::AutoReset<EditorServiceConnector::LaunchCallback*> launch_override_ =
      EditorServiceConnector::SetLaunchOverrideForTesting(&launch_callback_);
};

TEST_F(EditorMediatorServiceConnectionTest,
       FeedbackFromServiceIsSentForProfileAccount) {
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  AnnotatedAccountId::Set(&profile(), account_id);
  EditorMediator mediator(
      TestingBrowserProcess::GetGlobal()
          ->GetFeatures()
          ->application_locale_storage(),
      &profile(), manta::MantaServiceFactory::GetForProfile(&profile()),
      std::make_unique<EditorGeolocationMockProvider>("us"));

  // Focusing a text field connects the mediator to the Orca service. The test
  // keeps only the SystemActuator remote and drops the other endpoints.
  mojo::PendingAssociatedRemote<orca::mojom::SystemActuator>
      pending_system_actuator;
  EXPECT_CALL(orca_service_, BindEditor)
      .WillOnce(MoveArg<0>(&pending_system_actuator));
  mediator.OnFocus(/*context_id=*/1);
  ASSERT_TRUE(
      base::test::RunUntil([&] { return pending_system_actuator.is_valid(); }));
  mojo::AssociatedRemote<orca::mojom::SystemActuator> system_actuator(
      std::move(pending_system_actuator));

  EXPECT_CALL(send_feedback_callback_,
              Run(account_id, feedback::kOrcaFeedbackProductId,
                  "test description", Eq(std::nullopt), Eq(std::nullopt)))
      .WillOnce(Return(true));

  system_actuator->SubmitFeedback("test description");
  system_actuator.FlushForTesting();
}

}  // namespace
}  // namespace ash::input_method
