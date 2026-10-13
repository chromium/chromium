// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_disabled_dialog.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

namespace chrome {

void ShowFeedbackDisabledDialog(gfx::NativeWindow parent, Profile* profile) {
  if (!base::FeatureList::IsEnabled(features::kFeedbackDisabledDialog)) {
    return;
  }
  CHECK(profile);

  FeedbackDisabledDialogParentStatus status =
      FeedbackDisabledDialogParentStatus::kDirectParent;
  // The disabled dialog is browser-modal and requires an anchoring parent
  // window. If the caller did not supply one (e.g. when triggered from WebUI
  // such as chrome://whats-new, contextual tasks, Global Media Controls, or
  // extensions), fall back to the most recently active browser window for this
  // profile.
  if (!parent) {
    if (auto* collection = ProfileBrowserCollection::GetForProfile(profile)) {
      BrowserWindowInterface* bwi =
          collection->FindTabbedBrowser(/*match_original_profiles=*/true);
      if (bwi && bwi->GetWindow()) {
        parent = bwi->GetWindow()->GetNativeWindow();
      }
    }
    status = parent ? FeedbackDisabledDialogParentStatus::kFoundByFallback
                    : FeedbackDisabledDialogParentStatus::kNotFound;
  }
  base::UmaHistogramEnumeration("Feedback.DisabledDialog.ParentStatus", status);
  if (!parent) {
    return;
  }

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
