// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_system_actuator.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "components/account_id/account_id.h"
#include "components/feedback/feedback_constants.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

class MockSystem : public EditorSystemActuator::System {
 public:
  MockSystem() = default;
  ~MockSystem() override = default;

  // EditorSystemActuator::System:
  MOCK_METHOD(void, Announce, (const std::u16string& message), (override));
  MOCK_METHOD(void,
              ProcessConsentAction,
              (ConsentAction consent_action),
              (override));
  MOCK_METHOD(void, ShowUI, (), (override));
  MOCK_METHOD(void, CloseUI, (), (override));
  MOCK_METHOD(void,
              HandleTrigger,
              (std::optional<std::string_view> preset_query_id,
               std::optional<std::string_view> freeform_text),
              (override));
  MOCK_METHOD(EditorMetricsRecorder*, GetMetricsRecorder, (), (override));
  MOCK_METHOD(size_t, GetSelectedTextLength, (), (override));
};

class EditorSystemActuatorTest : public ChromeAshTestBase {
 public:
  EditorSystemActuatorTest() = default;
  ~EditorSystemActuatorTest() override = default;

 protected:
  // ChromeAshTestBase:
  void SetUpInitParams(AshTestHelper::InitParams& init_params) override {
    ChromeAshTestBase::SetUpInitParams(init_params);
    auto shell_delegate = std::make_unique<TestShellDelegate>();
    shell_delegate->SetSendSpecializedFeatureFeedbackCallback(
        send_feedback_callback_.Get());
    init_params.delegate = std::move(shell_delegate);
  }

  base::MockCallback<TestShellDelegate::SendSpecializedFeatureFeedbackCallback>
      send_feedback_callback_;
};

TEST_F(EditorSystemActuatorTest, SubmitFeedbackSendsOrcaFeedbackForAccount) {
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  NiceMock<MockSystem> system;
  mojo::PendingAssociatedRemote<orca::mojom::SystemActuator> remote;
  EditorSystemActuator system_actuator(
      account_id, remote.InitWithNewEndpointAndPassReceiver(), &system);

  EXPECT_CALL(send_feedback_callback_,
              Run(account_id, feedback::kOrcaFeedbackProductId,
                  "test description", Eq(std::nullopt), Eq(std::nullopt)))
      .WillOnce(Return(true));

  system_actuator.SubmitFeedback("test description");
}

}  // namespace
}  // namespace ash::input_method
