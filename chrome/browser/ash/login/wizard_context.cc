// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/wizard_context.h"

#include "build/branding_buildflags.h"
#include "chromeos/login/auth/user_context.h"

namespace ash {

WizardContext::WizardContext() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  is_branded_build = true;
#else
  is_branded_build = false;
#endif
}

WizardContext::~WizardContext() = default;

}  // namespace ash
