// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/accessibility_labels_bubble_model.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace {

// static
void RecordModalDialogAccepted(bool accepted) {
  UMA_HISTOGRAM_BOOLEAN("Accessibility.ImageLabels.ModalDialogAccepted",
                        accepted);
}

}  // namespace

AccessibilityLabelsBubbleModel::AccessibilityLabelsBubbleModel(
    Profile* profile,
    WebContents* web_contents,
    bool enable_always)
    : profile_(profile),
      web_contents_(web_contents),
      enable_always_(enable_always) {}

AccessibilityLabelsBubbleModel::~AccessibilityLabelsBubbleModel() {}

base::string16 AccessibilityLabelsBubbleModel::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_DIALOG_TITLE);
}

base::string16 AccessibilityLabelsBubbleModel::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      enable_always_
          ? IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_BUBBLE_TEXT
          : IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_BUBBLE_TEXT_ONCE);
}

base::string16 AccessibilityLabelsBubbleModel::GetButtonLabel(
    BubbleButton button) const {
  return l10n_util::GetStringUTF16(
      button == BUTTON_OK
          ? IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_BUBBLE_ENABLE
          : IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_BUBBLE_DISABLE);
}

void AccessibilityLabelsBubbleModel::Accept() {
  // Note that the user has already seen and accepted this opt-in dialog.
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityImageLabelsOptInAccepted, true);
  RecordModalDialogAccepted(true);

  if (enable_always_) {
    SetPref(true);
    return;
  }
  AccessibilityLabelsServiceFactory::GetForProfile(profile_)
      ->EnableLabelsServiceOnce();
}

void AccessibilityLabelsBubbleModel::Cancel() {
  if (enable_always_)
    SetPref(false);
  // If not enable_always_, canceling does not have any impact.
  RecordModalDialogAccepted(/* not accepted */ false);
}

base::string16 AccessibilityLabelsBubbleModel::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL AccessibilityLabelsBubbleModel::GetHelpPageURL() const {
  return GURL(chrome::kAccessibilityLabelsLearnMoreURL);
}

void AccessibilityLabelsBubbleModel::OpenHelpPage() {
  // TODO(katie): Link to a specific accessibility labels help page when
  // there is one; check with the privacy team.
  OpenURLParams params(GetHelpPageURL(), Referrer(),
                       WindowOpenDisposition::NEW_FOREGROUND_TAB,
                       ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params);
}

void AccessibilityLabelsBubbleModel::SetPref(bool enabled) {
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kAccessibilityImageLabelsEnabled, enabled);
}
