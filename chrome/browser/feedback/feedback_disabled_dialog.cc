// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_disabled_dialog.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

namespace chrome {

void ShowFeedbackDisabledDialog(gfx::NativeWindow parent,
                                const Profile* profile) {
  CHECK(parent);
  CHECK(profile);
  // Feedback can be disabled either by enterprise policy (kUserFeedbackAllowed
  // is false) or by account capabilities (e.g., underaged account).
  const int body_message_id =
      profile->GetPrefs()->GetBoolean(prefs::kUserFeedbackAllowed)
          ? IDS_FEEDBACK_DISABLED_DIALOG_BODY
          : IDS_FEEDBACK_DISABLED_DIALOG_BODY_ENTERPRISE;

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetInternalName("FeedbackDisabledDialog")
          .SetTitle(
              l10n_util::GetStringUTF16(IDS_FEEDBACK_DISABLED_DIALOG_TITLE))
          .AddParagraph(ui::DialogModelLabel(body_message_id))
          .AddOkButton(base::DoNothing(),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_CLOSE)))
          .Build();

  constrained_window::ShowBrowserModal(std::move(dialog_model), parent);
}

}  // namespace chrome
