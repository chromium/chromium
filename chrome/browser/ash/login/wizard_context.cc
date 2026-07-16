// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/wizard_context.h"

#include "ash/constants/ash_login_pref_names.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ui/webui/ash/login/family_link_notice_screen_handler.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash {

bool WizardContext::g_is_branded_build =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    true;
#else
    false;
#endif

WizardContext::WizardContext()
    : screen_after_managed_tos(FamilyLinkNoticeView::kScreenId) {}

WizardContext::~WizardContext() = default;

WizardContext::GaiaConfig::GaiaConfig() = default;
WizardContext::GaiaConfig::~GaiaConfig() = default;

bool IsRollbackFlow(const WizardContext& context) {
  return context.configuration.FindBool(configuration::kRestoreAfterRollback)
      .value_or(false);
}

bool WizardContext::ShouldTriggerAutoWipe(PrefService& local_state) const {
  if (!user_context) {
    return false;
  }

  // Consumer accounts should never be automatically wiped. We verify the user's
  // enterprise-managed status directly from the pre-populated KnownUser list in
  // Local State to ensure safety before applying any destructive policies.
  // Note: For ephemeral managed users, this value evaluates to false. However,
  // since ephemeral users are automatically wiped upon logout/reboot anyway,
  // skipping the explicit AutoWipe flow here is completely safe and correct.
  user_manager::KnownUser known_user(&local_state);
  if (!known_user.GetIsEnterpriseManaged(user_context->GetAccountId())) {
    return false;
  }

  // Only proceed with the wipe if the administrator has explicitly configured
  // the device to automatically wipe the cryptohome on an online password
  // mismatch.
  return local_state.GetInteger(prefs::kDeviceOnlinePasswordMismatchBehavior) ==
         static_cast<int>(DeviceOnlinePasswordMismatchBehavior::kAutoWipe);
}

void SetUserContext(WizardContext& wizard_context,
                    std::unique_ptr<UserContext> user_context) {
  CHECK(!wizard_context.user_context);
  CHECK(!wizard_context.timebound_user_context_holder);
  wizard_context.user_context = std::move(user_context);
}

}  // namespace ash
