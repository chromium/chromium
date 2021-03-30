// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_screenlock_state_handler.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_metrics.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_manager.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/components/proximity_auth/screenlock_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using proximity_auth::ScreenlockState;

namespace chromeos {
namespace {

// Icons used by EasyUnlockScreenlockStateHandler. The icon id values are the
// same as the ones set by proximity_auth::ScreenlockBridge.
const char kLockedIconId[] = "locked";
const char kLockedToBeActivatedIconId[] = "locked-to-be-activated";
const char kUnlockedIconId[] = "unlocked";
const char kSpinnerIconId[] = "spinner";
const char kHardlockedIconId[] = "hardlocked";

class FakeProximityAuthPrefManager
    : public proximity_auth::ProximityAuthPrefManager {
 public:
  FakeProximityAuthPrefManager() = default;
  ~FakeProximityAuthPrefManager() override = default;

  // proximity_auth::ProximityAuthPrefManager:
  bool IsEasyUnlockAllowed() const override { return true; }

  bool IsEasyUnlockEnabled() const override { return true; }
  void SetIsEasyUnlockEnabled(bool is_easy_unlock_enabled) const override {}

  bool IsEasyUnlockEnabledStateSet() const override { return true; }
  void SetEasyUnlockEnabledStateSet() const override {}

  bool IsChromeOSLoginAllowed() const override { return true; }

  bool IsChromeOSLoginEnabled() const override { return true; }
  void SetIsChromeOSLoginEnabled(bool is_enabled) override {}

  int64_t GetLastPromotionCheckTimestampMs() const override { return 0; }
  void SetLastPromotionCheckTimestampMs(int64_t timestamp_ms) override {}

  int GetPromotionShownCount() const override { return 0; }
  void SetPromotionShownCount(int count) override {}

  bool HasShownLoginDisabledMessage() const override {
    return has_shown_login_disabled_message_;
  }
  void SetHasShownLoginDisabledMessage(bool has_shown) override {
    has_shown_login_disabled_message_ = has_shown;
  }

 private:
  bool has_shown_login_disabled_message_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeProximityAuthPrefManager);
};

// Checks if `input` string has any unreplaced placeholders.
bool StringHasPlaceholders(const std::u16string& input) {
  std::vector<size_t> offsets;
  std::vector<std::u16string> subst;
  subst.push_back(std::u16string());

  std::u16string replaced =
      base::ReplaceStringPlaceholders(input, subst, &offsets);
  return !offsets.empty();
}

// Fake lock handler to be used in these tests.
class TestLockHandler : public proximity_auth::ScreenlockBridge::LockHandler {
 public:
  explicit TestLockHandler(const AccountId& account_id)
      : account_id_(account_id),
        show_icon_count_(0u),
        auth_type_(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD) {}
  ~TestLockHandler() override {}

  // proximity_auth::ScreenlockBridge::LockHandler implementation:
  void ShowBannerMessage(const std::u16string& message,
                         bool is_warning) override {
    ASSERT_FALSE(true) << "Should not be reached.";
  }

  void ShowUserPodCustomIcon(
      const AccountId& account_id,
      const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions& icon)
      override {
    ASSERT_TRUE(account_id_ == account_id)
        << "account_id_=" << account_id_.Serialize()
        << " != account_id=" << account_id.Serialize();
    ++show_icon_count_;
    last_custom_icon_ = icon.ToDictionaryValue();
    ValidateCustomIcon();
  }

  void HideUserPodCustomIcon(const AccountId& account_id) override {
    ASSERT_TRUE(account_id_ == account_id)
        << "account_id_=" << account_id_.Serialize()
        << " != account_id=" << account_id.Serialize();
    last_custom_icon_.reset();
  }

  void EnableInput() override {
    ASSERT_FALSE(true) << "Should not be reached.";
  }

  void SetAuthType(const AccountId& account_id,
                   proximity_auth::mojom::AuthType auth_type,
                   const std::u16string& auth_value) override {
    ASSERT_TRUE(account_id_ == account_id)
        << "account_id_=" << account_id_.Serialize()
        << " != account_id=" << account_id.Serialize();
    // Generally, this is allowed, but EasyUnlockScreenlockStateHandler should
    // avoid resetting the same auth type.
    EXPECT_NE(auth_type_, auth_type);

    auth_type_ = auth_type;
    auth_value_ = auth_value;
  }

  proximity_auth::mojom::AuthType GetAuthType(
      const AccountId& account_id) const override {
    EXPECT_TRUE(account_id_ == account_id)
        << "account_id_=" << account_id_.Serialize()
        << " != account_id=" << account_id.Serialize();
    return auth_type_;
  }

  ScreenType GetScreenType() const override { return LOCK_SCREEN; }

  void Unlock(const AccountId& account_id) override {
    ASSERT_FALSE(true) << "Should not be reached.";
  }

  void AttemptEasySignin(const AccountId& account_id,
                         const std::string& secret,
                         const std::string& key_label) override {
    ASSERT_FALSE(true) << "Should not be reached.";
  }

  // Utility methods used by tests:

  // Gets last set auth value.
  std::u16string GetAuthValue() const { return auth_value_; }

  // Sets the auth value.
  void SetAuthValue(const std::u16string& value) { auth_value_ = value; }

  // Returns the number of times an icon was shown since the last call to this
  // method.
  size_t GetAndResetShowIconCount() {
    size_t result = show_icon_count_;
    show_icon_count_ = 0u;
    return result;
  }

  // Whether the custom icon is set.
  bool HasCustomIcon() const { return !!last_custom_icon_; }

  // If custom icon is set, returns the icon's id.
  // If there is no icon, or if it doesn't have an id set, returns an empty
  // string.
  std::string GetCustomIconId() const {
    std::string result;
    if (last_custom_icon_)
      last_custom_icon_->GetString("id", &result);
    return result;
  }

  // Whether the custom icon is set and it has a tooltip.
  bool CustomIconHasTooltip() const {
    return last_custom_icon_ && last_custom_icon_->HasKey("tooltip");
  }

  // Gets the custom icon's tooltip text, if one is set.
  std::u16string GetCustomIconTooltip() const {
    std::u16string result;
    if (last_custom_icon_)
      last_custom_icon_->GetString("tooltip.text", &result);
    return result;
  }

  // Whether the custom icon's tooltip should be autoshown. If the icon is not
  // set, or it doesn't have a tooltip, returns false.
  bool IsCustomIconTooltipAutoshown() const {
    bool result = false;
    if (last_custom_icon_)
      last_custom_icon_->GetBoolean("tooltip.autoshow", &result);
    return result;
  }

  // Whether the custom icon is set and if has hardlock capability enabed.
  bool CustomIconHardlocksOnClick() const {
    bool result = false;
    if (last_custom_icon_)
      last_custom_icon_->GetBoolean("hardlockOnClick", &result);
    return result;
  }

 private:
  // Does some sanity checks on the last icon set by `ShowUserPodCustomIcon`.
  // It will cause a test failure if the icon is not valid.
  void ValidateCustomIcon() {
    ASSERT_TRUE(last_custom_icon_.get());

    EXPECT_TRUE(last_custom_icon_->HasKey("id"));

    if (last_custom_icon_->HasKey("tooltip")) {
      std::u16string tooltip;
      EXPECT_TRUE(last_custom_icon_->GetString("tooltip.text", &tooltip));
      EXPECT_FALSE(tooltip.empty());
      EXPECT_FALSE(StringHasPlaceholders(tooltip));
    }
  }

  // The fake account id used in test. All methods called on `this` should be
  // associated with this user.
  const AccountId account_id_;

  // The last icon set using `SetUserPodCustomIcon`. Call to
  // `HideUserPodcustomIcon` resets it.
  std::unique_ptr<base::DictionaryValue> last_custom_icon_;
  size_t show_icon_count_;

  // Auth type and value set using `SetAuthType`.
  proximity_auth::mojom::AuthType auth_type_;
  std::u16string auth_value_;

  DISALLOW_COPY_AND_ASSIGN(TestLockHandler);
};

class EasyUnlockScreenlockStateHandlerTest : public testing::Test {
 public:
  EasyUnlockScreenlockStateHandlerTest() {}
  ~EasyUnlockScreenlockStateHandlerTest() override {}

  void SetUp() override {
    // Create and inject fake lock handler to the screenlock bridge.
    lock_handler_.reset(new TestLockHandler(account_id_));
    proximity_auth::ScreenlockBridge* screenlock_bridge =
        proximity_auth::ScreenlockBridge::Get();
    screenlock_bridge->SetLockHandler(lock_handler_.get());
    fake_pref_manager_ = std::make_unique<FakeProximityAuthPrefManager>();

    // Create the screenlock state handler object that will be tested.
    state_handler_.reset(new EasyUnlockScreenlockStateHandler(
        account_id_, EasyUnlockScreenlockStateHandler::NO_HARDLOCK,
        screenlock_bridge, fake_pref_manager_.get()));
  }

  void TearDown() override {
    proximity_auth::ScreenlockBridge::Get()->SetLockHandler(NULL);
    lock_handler_.reset();
    state_handler_.reset();
  }

 protected:
  // The state handler that is being tested.
  std::unique_ptr<EasyUnlockScreenlockStateHandler> state_handler_;

  // The user associated with `state_handler_`.
  const AccountId account_id_ = AccountId::FromUserEmail("test_user@gmail.com");

  std::unique_ptr<FakeProximityAuthPrefManager> fake_pref_manager_;

  // Faked lock handler given to proximity_auth::ScreenlockBridge during the
  // test. Abstracts the screen lock UI.
  std::unique_ptr<TestLockHandler> lock_handler_;
};

TEST_F(EasyUnlockScreenlockStateHandlerTest, AuthenticatedNotInitialRun) {
  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::USER_CLICK,
            lock_handler_->GetAuthType(account_id_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kUnlockedIconId, lock_handler_->GetCustomIconId());
  EXPECT_TRUE(lock_handler_->CustomIconHasTooltip());
  EXPECT_FALSE(lock_handler_->IsCustomIconTooltipAutoshown());
  EXPECT_TRUE(lock_handler_->CustomIconHardlocksOnClick());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, IsActive) {
  EXPECT_FALSE(state_handler_->IsActive());
  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);
  EXPECT_TRUE(state_handler_->IsActive());
  state_handler_->ChangeState(ScreenlockState::INACTIVE);
  EXPECT_FALSE(state_handler_->IsActive());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, BluetoothConnecting) {
  state_handler_->ChangeState(ScreenlockState::BLUETOOTH_CONNECTING);
  EXPECT_TRUE(state_handler_->IsActive());

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kSpinnerIconId, lock_handler_->GetCustomIconId());
  EXPECT_FALSE(lock_handler_->CustomIconHasTooltip());
  EXPECT_TRUE(lock_handler_->CustomIconHardlocksOnClick());

  state_handler_->ChangeState(ScreenlockState::BLUETOOTH_CONNECTING);
  // Duplicated state change should be ignored.
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, HardlockedState) {
  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::USER_CLICK,
            lock_handler_->GetAuthType(account_id_));

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::USER_HARDLOCK);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kHardlockedIconId, lock_handler_->GetCustomIconId());
  EXPECT_TRUE(lock_handler_->CustomIconHasTooltip());
  EXPECT_TRUE(lock_handler_->IsCustomIconTooltipAutoshown());
  EXPECT_FALSE(lock_handler_->CustomIconHardlocksOnClick());

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::USER_HARDLOCK);

  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       LoginDisabled_HasNotYetShownMessage) {
  fake_pref_manager_->SetHasShownLoginDisabledMessage(false);

  state_handler_->ChangeState(ScreenlockState::NO_PHONE);
  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::LOGIN_DISABLED);

  EXPECT_EQ(2u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kHardlockedIconId, lock_handler_->GetCustomIconId());
  EXPECT_TRUE(lock_handler_->CustomIconHasTooltip());
  EXPECT_FALSE(lock_handler_->CustomIconHardlocksOnClick());

  EXPECT_TRUE(lock_handler_->IsCustomIconTooltipAutoshown());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       LoginDisabled_HasAlreadyShownMessage) {
  fake_pref_manager_->SetHasShownLoginDisabledMessage(true);

  state_handler_->ChangeState(ScreenlockState::NO_PHONE);
  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::LOGIN_DISABLED);

  EXPECT_EQ(2u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kHardlockedIconId, lock_handler_->GetCustomIconId());
  EXPECT_TRUE(lock_handler_->CustomIconHasTooltip());
  EXPECT_FALSE(lock_handler_->CustomIconHardlocksOnClick());

  EXPECT_FALSE(lock_handler_->IsCustomIconTooltipAutoshown());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, HardlockedStateNoPairing) {
  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::USER_CLICK,
            lock_handler_->GetAuthType(account_id_));

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::NO_PAIRING);

  EXPECT_FALSE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, StatesWithLockedIcon) {
  std::vector<ScreenlockState> states;
  states.push_back(ScreenlockState::NO_BLUETOOTH);
  states.push_back(ScreenlockState::NO_PHONE);
  states.push_back(ScreenlockState::PHONE_NOT_LOCKABLE);
  states.push_back(ScreenlockState::PHONE_NOT_AUTHENTICATED);
  states.push_back(ScreenlockState::PHONE_LOCKED);

  for (size_t i = 0; i < states.size(); ++i) {
    SCOPED_TRACE(base::NumberToString(i));
    state_handler_->ChangeState(states[i]);
    EXPECT_TRUE(state_handler_->IsActive());

    EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
    EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
              lock_handler_->GetAuthType(account_id_));

    ASSERT_TRUE(lock_handler_->HasCustomIcon());
    EXPECT_EQ(kLockedIconId, lock_handler_->GetCustomIconId());
    EXPECT_TRUE(lock_handler_->CustomIconHasTooltip());
    EXPECT_TRUE(lock_handler_->IsCustomIconTooltipAutoshown());
    EXPECT_TRUE(lock_handler_->CustomIconHardlocksOnClick());

    state_handler_->ChangeState(states[i]);
    EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  }
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       LockScreenClearedOnStateHandlerDestruction) {
  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::USER_CLICK,
            lock_handler_->GetAuthType(account_id_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());

  state_handler_.reset();

  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));

  ASSERT_FALSE(lock_handler_->HasCustomIcon());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, StatePreservedWhenScreenUnlocks) {
  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::USER_CLICK,
            lock_handler_->GetAuthType(account_id_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());

  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(NULL);
  lock_handler_.reset(new TestLockHandler(account_id_));
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::USER_CLICK,
            lock_handler_->GetAuthType(account_id_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, StateChangeWhileScreenUnlocked) {
  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::USER_CLICK,
            lock_handler_->GetAuthType(account_id_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());

  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(NULL);
  lock_handler_.reset(new TestLockHandler(account_id_));
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());

  state_handler_->ChangeState(ScreenlockState::BLUETOOTH_CONNECTING);

  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kSpinnerIconId, lock_handler_->GetCustomIconId());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, NoPairingHardlockClearsIcon) {
  state_handler_->ChangeState(ScreenlockState::PHONE_LOCKED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kLockedIconId, lock_handler_->GetCustomIconId());

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::NO_PAIRING);

  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_FALSE(lock_handler_->HasCustomIcon());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, PairingChangedHardlock) {
  state_handler_->ChangeState(ScreenlockState::PHONE_LOCKED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kLockedIconId, lock_handler_->GetCustomIconId());

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::PAIRING_CHANGED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kLockedToBeActivatedIconId, lock_handler_->GetCustomIconId());

  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);

  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kLockedToBeActivatedIconId, lock_handler_->GetCustomIconId());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, InactiveStateHidesIcon) {
  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);

  ASSERT_TRUE(lock_handler_->HasCustomIcon());

  state_handler_->ChangeState(ScreenlockState::INACTIVE);

  ASSERT_FALSE(lock_handler_->HasCustomIcon());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       AuthenticatedStateClearsPreviousAuthValue) {
  state_handler_->ChangeState(ScreenlockState::INACTIVE);

  lock_handler_->SetAuthValue(u"xxx");

  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_SCREENLOCK_USER_POD_AUTH_VALUE),
      lock_handler_->GetAuthValue());

  state_handler_->ChangeState(ScreenlockState::NO_PHONE);

  EXPECT_EQ(std::u16string(), lock_handler_->GetAuthValue());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       ChangingStateDoesNotAffectAuthValueIfAuthTypeDoesNotChange) {
  lock_handler_->SetAuthValue(u"xxx");

  state_handler_->ChangeState(ScreenlockState::NO_PHONE);
  EXPECT_EQ(u"xxx", lock_handler_->GetAuthValue());

  state_handler_->ChangeState(ScreenlockState::PHONE_NOT_AUTHENTICATED);
  EXPECT_EQ(u"xxx", lock_handler_->GetAuthValue());

  state_handler_->ChangeState(ScreenlockState::BLUETOOTH_CONNECTING);
  EXPECT_EQ(u"xxx", lock_handler_->GetAuthValue());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kSpinnerIconId, lock_handler_->GetCustomIconId());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, StateChangesIgnoredIfHardlocked) {
  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::USER_CLICK,
            lock_handler_->GetAuthType(account_id_));

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::USER_HARDLOCK);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kHardlockedIconId, lock_handler_->GetCustomIconId());

  state_handler_->ChangeState(ScreenlockState::NO_PHONE);
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());

  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       LockScreenChangeableOnLockAfterHardlockReset) {
  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::USER_HARDLOCK);
  EXPECT_EQ(2u, lock_handler_->GetAndResetShowIconCount());

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::NO_HARDLOCK);

  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(NULL);
  lock_handler_.reset(new TestLockHandler(account_id_));
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());

  state_handler_->ChangeState(ScreenlockState::NO_PHONE);

  EXPECT_EQ(2u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_TRUE(lock_handler_->HasCustomIcon());

  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(NULL);
  lock_handler_.reset(new TestLockHandler(account_id_));
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));
  EXPECT_EQ(kLockedIconId, lock_handler_->GetCustomIconId());

  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);
  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(proximity_auth::mojom::AuthType::USER_CLICK,
            lock_handler_->GetAuthType(account_id_));
  EXPECT_TRUE(lock_handler_->CustomIconHardlocksOnClick());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, HardlockStatePersistsOverUnlocks) {
  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);
  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::USER_HARDLOCK);
  EXPECT_EQ(2u, lock_handler_->GetAndResetShowIconCount());

  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(NULL);
  lock_handler_.reset(new TestLockHandler(account_id_));
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kHardlockedIconId, lock_handler_->GetCustomIconId());

  state_handler_->ChangeState(ScreenlockState::AUTHENTICATED);
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(account_id_));
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, PrimaryUserAbsent) {
  state_handler_->ChangeState(ScreenlockState::PRIMARY_USER_ABSENT);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kLockedIconId, lock_handler_->GetCustomIconId());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, NoOverrideOnlineSignin) {
  lock_handler_->SetAuthType(account_id_,
                             proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
                             std::u16string());

  std::vector<ScreenlockState> states;
  states.push_back(ScreenlockState::NO_BLUETOOTH);
  states.push_back(ScreenlockState::NO_PHONE);
  states.push_back(ScreenlockState::PHONE_NOT_LOCKABLE);
  states.push_back(ScreenlockState::PHONE_NOT_AUTHENTICATED);
  states.push_back(ScreenlockState::PHONE_LOCKED);
  states.push_back(ScreenlockState::PHONE_NOT_LOCKABLE);
  states.push_back(ScreenlockState::RSSI_TOO_LOW);
  states.push_back(ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW);
  states.push_back(ScreenlockState::AUTHENTICATED);

  for (size_t i = 0; i < states.size(); ++i) {
    state_handler_->ChangeState(states[i]);
    EXPECT_EQ(proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
              lock_handler_->GetAuthType(account_id_));
    EXPECT_FALSE(lock_handler_->HasCustomIcon());
  }

  std::vector<EasyUnlockScreenlockStateHandler::HardlockState> hardlock_states;
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::NO_HARDLOCK);
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::USER_HARDLOCK);
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::PAIRING_CHANGED);
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::PAIRING_ADDED);
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::NO_PAIRING);
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::LOGIN_FAILED);

  for (size_t i = 0; i < hardlock_states.size(); ++i) {
    state_handler_->SetHardlockState(hardlock_states[i]);
    EXPECT_EQ(proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
              lock_handler_->GetAuthType(account_id_));
    EXPECT_FALSE(lock_handler_->HasCustomIcon());
  }
}

}  // namespace
}  // namespace chromeos
