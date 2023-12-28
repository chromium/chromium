// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/fingerprint_auth_factor_model.h"
#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/login/ui/fake_fingerprint_auth_factor_model.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"

namespace ash {

namespace {

using AuthFactorState = AuthFactorModel::AuthFactorState;

struct Testcase {
  FingerprintState fingerprint_state;
  AuthFactorState auth_factor_state;
  int label_id;
};

constexpr Testcase kTestCases[] = {
    {FingerprintState::AVAILABLE_DEFAULT, AuthFactorState::kReady,
     IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AVAILABLE},
    {FingerprintState::UNAVAILABLE, AuthFactorState::kUnavailable,
     IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AVAILABLE},
    {FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING,
     AuthFactorState::kErrorTemporary,
     IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_TOUCH_SENSOR},
    {FingerprintState::DISABLED_FROM_ATTEMPTS, AuthFactorState::kErrorPermanent,
     IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_DISABLED_FROM_ATTEMPTS},
    {FingerprintState::DISABLED_FROM_TIMEOUT, AuthFactorState::kErrorPermanent,
     IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED}};
}  // namespace

class FingerprintAuthFactorModelTest : public AshTestBase {
 public:
  FingerprintAuthFactorModelTest() = default;
  FingerprintAuthFactorModelTest(const FingerprintAuthFactorModelTest&) =
      delete;
  FingerprintAuthFactorModelTest& operator=(
      const FingerprintAuthFactorModelTest&) = delete;
  ~FingerprintAuthFactorModelTest() override = default;

 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    fake_fingerprint_auth_factor_model_factory_ =
        std::make_unique<FakeFingerprintAuthFactorModelFactory>();
    FingerprintAuthFactorModel::Factory::SetFactoryForTesting(
        fake_fingerprint_auth_factor_model_factory_.get());
    fingerprint_auth_factor_model_ =
        FingerprintAuthFactorModel::Factory::Create(
            FingerprintState::UNAVAILABLE);
    model_ = fingerprint_auth_factor_model_.get();

    model_->Init(&icon_, base::BindRepeating(
                             &FingerprintAuthFactorModelTest::OnStateChanged,
                             base::Unretained(this)));
  }

  void TearDown() override {
    FingerprintAuthFactorModel::Factory::SetFactoryForTesting(nullptr);
    AshTestBase::TearDown();
  }

  void OnStateChanged() { on_state_changed_called_ = true; }

  std::unique_ptr<FakeFingerprintAuthFactorModelFactory>
      fake_fingerprint_auth_factor_model_factory_;
  std::unique_ptr<FingerprintAuthFactorModel> fingerprint_auth_factor_model_;
  AuthIconView icon_;
  raw_ptr<AuthFactorModel> model_ = nullptr;
  bool on_state_changed_called_ = false;
};

TEST_F(FingerprintAuthFactorModelTest, GetType) {
  EXPECT_EQ(AuthFactorType::kFingerprint, model_->GetType());
}

TEST_F(FingerprintAuthFactorModelTest, GetAuthFactorState) {
  for (const Testcase& testcase : kTestCases) {
    fingerprint_auth_factor_model_->SetFingerprintState(
        testcase.fingerprint_state);
    EXPECT_EQ(model_->GetAuthFactorState(), testcase.auth_factor_state);
  }
}

TEST_F(FingerprintAuthFactorModelTest, GetAuthFactorState_Unavailable) {
  for (const Testcase& testcase : kTestCases) {
    fingerprint_auth_factor_model_->SetFingerprintState(
        testcase.fingerprint_state);
    fingerprint_auth_factor_model_->set_available(false);
    EXPECT_EQ(model_->GetAuthFactorState(), AuthFactorState::kUnavailable);
  }
}

TEST_F(FingerprintAuthFactorModelTest, GetAuthFactorState_AuthResultFalse) {
  for (const Testcase& testcase : kTestCases) {
    fingerprint_auth_factor_model_->SetFingerprintState(
        testcase.fingerprint_state);
    fingerprint_auth_factor_model_->NotifyFingerprintAuthResult(false);
    if (testcase.fingerprint_state ==
        FingerprintState::DISABLED_FROM_ATTEMPTS) {
      EXPECT_EQ(model_->GetAuthFactorState(), AuthFactorState::kErrorPermanent);
    } else {
      EXPECT_EQ(model_->GetAuthFactorState(), AuthFactorState::kErrorTemporary);
    }
  }
}

TEST_F(FingerprintAuthFactorModelTest, GetAuthFactorState_AuthResulTrue) {
  for (const Testcase& testcase : kTestCases) {
    fingerprint_auth_factor_model_->SetFingerprintState(
        testcase.fingerprint_state);
    fingerprint_auth_factor_model_->NotifyFingerprintAuthResult(true);
    EXPECT_EQ(model_->GetAuthFactorState(), AuthFactorState::kAuthenticated);
  }
}

TEST_F(FingerprintAuthFactorModelTest, GetLabelId) {
  for (const Testcase& testcase : kTestCases) {
    fingerprint_auth_factor_model_->SetFingerprintState(
        testcase.fingerprint_state);
    EXPECT_EQ(model_->GetLabelId(), testcase.label_id);
  }
}

TEST_F(FingerprintAuthFactorModelTest, GetLabelId_AuthResultFalse) {
  for (const Testcase& testcase : kTestCases) {
    fingerprint_auth_factor_model_->SetFingerprintState(
        testcase.fingerprint_state);
    fingerprint_auth_factor_model_->NotifyFingerprintAuthResult(false);
    if (testcase.fingerprint_state ==
        FingerprintState::DISABLED_FROM_ATTEMPTS) {
      EXPECT_EQ(model_->GetLabelId(),
                IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_DISABLED_FROM_ATTEMPTS);
    } else {
      EXPECT_EQ(model_->GetLabelId(),
                IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_FAILED);
    }
  }
}

TEST_F(FingerprintAuthFactorModelTest, GetLabelId_AuthResultTrue) {
  for (const Testcase& testcase : kTestCases) {
    fingerprint_auth_factor_model_->SetFingerprintState(
        testcase.fingerprint_state);
    fingerprint_auth_factor_model_->NotifyFingerprintAuthResult(true);
    EXPECT_EQ(model_->GetLabelId(),
              IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_SUCCESS);
  }
}

TEST_F(FingerprintAuthFactorModelTest, GetLabelId_CanUsePin) {
  fingerprint_auth_factor_model_->SetFingerprintState(
      FingerprintState::DISABLED_FROM_TIMEOUT);
  model_->set_can_use_pin(true);
  EXPECT_EQ(model_->GetLabelId(),
            IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED);
}

TEST_F(FingerprintAuthFactorModelTest, ShouldAnnounceLabel) {
  for (const Testcase& testcase : kTestCases) {
    fingerprint_auth_factor_model_->SetFingerprintState(
        testcase.fingerprint_state);
    if (testcase.fingerprint_state ==
            FingerprintState::DISABLED_FROM_ATTEMPTS ||
        testcase.fingerprint_state == FingerprintState::DISABLED_FROM_TIMEOUT) {
      EXPECT_EQ(model_->ShouldAnnounceLabel(), true);
    } else {
      fingerprint_auth_factor_model_->NotifyFingerprintAuthResult(true);
      EXPECT_EQ(model_->ShouldAnnounceLabel(), false);
      fingerprint_auth_factor_model_->NotifyFingerprintAuthResult(false);
      EXPECT_EQ(model_->ShouldAnnounceLabel(), true);
    }
  }
}

TEST_F(FingerprintAuthFactorModelTest, GetAccessibleNameId) {
  for (const Testcase& testcase : kTestCases) {
    fingerprint_auth_factor_model_->SetFingerprintState(
        testcase.fingerprint_state);
    if (testcase.fingerprint_state !=
        FingerprintState::DISABLED_FROM_ATTEMPTS) {
      EXPECT_EQ(model_->GetAccessibleNameId(), testcase.label_id);
    } else {
      EXPECT_EQ(
          model_->GetAccessibleNameId(),
          IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_ACCESSIBLE_AUTH_DISABLED_FROM_ATTEMPTS);
    }
  }
}

TEST_F(FingerprintAuthFactorModelTest, DoHandleTapOrClick) {
  for (const Testcase& testcase : kTestCases) {
    fingerprint_auth_factor_model_->SetFingerprintState(
        testcase.fingerprint_state);
    model_->HandleTapOrClick();
    if (testcase.fingerprint_state == FingerprintState::AVAILABLE_DEFAULT) {
      EXPECT_EQ(model_->GetAuthFactorState(), AuthFactorState::kErrorTemporary);
    } else {
      EXPECT_EQ(model_->GetAuthFactorState(), testcase.auth_factor_state);
    }
  }
}

TEST_F(FingerprintAuthFactorModelTest, DoHandleErrorTimeout) {
  for (const Testcase& testcase : kTestCases) {
    fingerprint_auth_factor_model_->SetFingerprintState(
        testcase.fingerprint_state);
    model_->HandleErrorTimeout();
    if (testcase.fingerprint_state ==
        FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING) {
      EXPECT_EQ(model_->GetAuthFactorState(), AuthFactorState::kReady);
    } else {
      EXPECT_EQ(model_->GetAuthFactorState(), testcase.auth_factor_state);
    }
    fingerprint_auth_factor_model_->NotifyFingerprintAuthResult(false);
    model_->HandleErrorTimeout();
    if (testcase.fingerprint_state ==
        FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING) {
      EXPECT_EQ(model_->GetAuthFactorState(), AuthFactorState::kReady);
    } else {
      EXPECT_EQ(model_->GetAuthFactorState(), testcase.auth_factor_state);
    }
  }
}

}  // namespace ash
