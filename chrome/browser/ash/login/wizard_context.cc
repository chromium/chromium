// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/wizard_context.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ui/webui/ash/login/family_link_notice_screen_handler.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

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

}  // namespace ash
