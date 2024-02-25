// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/share_action/share_action.h"

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/drive/drive_api_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#endif

namespace sharesheet {

bool ShareAction::HasActionView() {
  return false;
}

bool ShareAction::ShouldShowAction(const apps::IntentPtr& intent,
                                   bool contains_hosted_document) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  for (const auto& file : intent->files) {
    if (file->mime_type &&
        drive::util::IsEncryptedMimeType(file->mime_type.value())) {
      return false;
    }
  }
  return !contains_hosted_document && intent && !intent->OnlyShareToDrive() &&
         intent->IsIntentValid();
#else
  return !contains_hosted_document;
#endif
}

bool ShareAction::OnAcceleratorPressed(const ui::Accelerator& accelerator) {
  return false;
}

void ShareAction::SetActionCleanupCallbackForArc(
    base::OnceCallback<void()> callback) {
  return;
}

}  // namespace sharesheet
