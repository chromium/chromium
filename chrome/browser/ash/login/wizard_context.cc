// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/wizard_context.h"

#include "ash/components/login/auth/user_context.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/webui/chromeos/login/family_link_notice_screen_handler.h"

namespace ash {

WizardContext::WizardContext()
    : screen_after_managed_tos(FamilyLinkNoticeView::kScreenId) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  is_branded_build = true;
#else
  is_branded_build = false;
#endif
}

WizardContext::~WizardContext() = default;

}  // namespace ash
