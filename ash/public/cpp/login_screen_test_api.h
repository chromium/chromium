// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_SCREEN_TEST_API_H_
#define ASH_PUBLIC_CPP_LOGIN_SCREEN_TEST_API_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/login_types.h"
#include "base/functional/callback_forward.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/geometry/rect.h"

class AccountId;

namespace ash {

class ASH_PUBLIC_EXPORT LoginScreenTestApi {
 public:
  LoginScreenTestApi() = delete;
  LoginScreenTestApi(const LoginScreenTestApi&) = delete;
  LoginScreenTestApi& operator=(const LoginScreenTestApi&) = delete;

  static bool IsLockShown();
  // Schedules the callback to be run when the LockScreen is shown. Note that
  // the LockScreen class is used for both the Lock and the Login screens.
  static void AddOnLockScreenShownCallback(
      base::OnceClosure on_lock_screen_shown);

  static bool IsLoginShelfShown();
  static bool IsRestartButtonShown();
  static bool IsShutdownButtonShown();
  static bool IsAppsButtonShown();
  static bool IsGuestButtonShown();
  static bool IsAddUserButtonShown();
  static bool IsCancelButtonShown();
  static bool IsParentAccessButtonShown();
  static bool IsEnterpriseEnrollmentButtonShown();
  static bool IsOsInstallButtonShown();
  static bool IsWarningBubbleShown();
  static bool IsUserAddingScreenIndicatorShown();
  static bool IsSystemInfoShown();
  static bool IsKioskDefaultMessageShown();
  static bool IsKioskInstructionBubbleShown();
  static bool IsPasswordFieldShown(const AccountId& account_id);
  static bool IsDisplayPasswordButtonShown(const AccountId& account_id);
  static bool IsManagedIconShown(const AccountId& account_id);
  static bool ShowRemoveAccountDialog(const AccountId& account_id);
  static bool IsManagedMessageInDialogShown(const AccountId& account_id);
  static bool IsForcedOnlineSignin(const AccountId& account_id);
  static void SubmitPassword(const AccountId& account_id,
                             const std::string& password,
                             bool check_if_submittable);
  static std::u16string GetChallengeResponseLabel(const AccountId& account_id);
  static bool IsChallengeResponseButtonClickable(const AccountId& account_id);
  static void ClickChallengeResponseButton(const AccountId& account_id);
  static int64_t GetUiUpdateCount();
  static bool LaunchApp(const std::string& app_id);
  static bool LaunchApp(const AccountId& account_id);
  static bool ClickAppsButton();
  static bool ClickAddUserButton();
  static bool ClickCancelButton();
  static bool ClickGuestButton();
  static bool ClickEnterpriseEnrollmentButton();
  static bool ClickOsInstallButton();
  static bool PressAccelerator(const ui::Accelerator& accelerator);
  static bool SendAcceleratorNatively(const ui::Accelerator& accelerator);
  static bool WaitForUiUpdate(int64_t previous_update_count);
  static int GetUsersCount();
  static bool FocusKioskDefaultMessage();
  static bool FocusUser(const AccountId& account_id);
  static AccountId GetFocusedUser();
  static bool RemoveUser(const AccountId& account_id);

  static std::string GetDisplayedName(const AccountId& account_id);
  static std::u16string GetDisabledAuthMessage(const AccountId& account_id);
  static std::u16string GetManagementDisclosureText(
      const AccountId& account_id);

  static bool ExpandPublicSessionPod(const AccountId& account_id);
  static bool HidePublicSessionExpandedPod();
  static bool IsPublicSessionExpanded();
  static bool IsExpandedPublicSessionAdvanced();
  static bool IsPublicSessionWarningShown();
  static void ClickPublicExpandedAdvancedViewButton();
  static void ClickPublicExpandedSubmitButton();
  static void SetPublicSessionLocale(const std::string& locale);
  static void SetPublicSessionKeyboard(const std::string& ime_id);
  static std::vector<ash::LocaleItem> GetPublicSessionLocales(
      const AccountId& account_id);
  static std::vector<ash::LocaleItem> GetExpandedPublicSessionLocales();
  static std::string GetExpandedPublicSessionSelectedLocale();
  static std::string GetExpandedPublicSessionSelectedKeyboard();

  static bool IsOobeDialogVisible();
  static std::u16string GetShutDownButtonLabel();
  static gfx::Rect GetShutDownButtonTargetBounds();
  static gfx::Rect GetShutDownButtonMirroredBounds();
  static std::string GetAppsButtonClassName();

  static void SetPinRequestWidgetShownCallback(
      base::RepeatingClosure on_pin_request_widget_shown);
  static std::u16string GetPinRequestWidgetTitle();
  static void SubmitPinRequestWidget(const std::string& pin);
  static void CancelPinRequestWidget();

  // Local authentication dialog methods.
  static bool IsLocalAuthenticationDialogVisible();
  static void CancelLocalAuthenticationDialog();
  static void SubmitPasswordLocalAuthenticationDialog(
      const std::string& password);

  // AuthErrorBubble methods.
  static bool IsAuthErrorBubbleShown();
  static void ShowAuthError(int unlock_attempt);
  static void HideAuthError();
  static void PressAuthErrorRecoveryButton();
  static void PressAuthErrorLearnMoreButton();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_SCREEN_TEST_API_H_
