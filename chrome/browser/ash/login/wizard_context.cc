// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/wizard_context.h"

#include "ash/components/login/auth/public/user_context.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/webui/chromeos/login/family_link_notice_screen_handler.h"

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

}  // namespace ash
