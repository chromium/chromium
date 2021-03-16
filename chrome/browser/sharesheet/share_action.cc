// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/share_action.h"

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/services/app_service/public/cpp/intent_util.h"
#endif

namespace sharesheet {

bool ShareAction::ShouldShowAction(const apps::mojom::IntentPtr& intent,
                                   bool contains_hosted_document) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return !contains_hosted_document && !apps_util::OnlyShareToDrive(intent) &&
         apps_util::IsIntentValid(intent);
#else
  return !contains_hosted_document;
#endif
}

bool ShareAction::OnAcceleratorPressed(const ui::Accelerator& accelerator) {
  return false;
}

}  // namespace sharesheet
