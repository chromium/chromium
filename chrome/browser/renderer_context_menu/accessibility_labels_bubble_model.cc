// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/accessibility_labels_bubble_model.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"

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
    content::WebContents* web_contents,
    bool enable_always)
    : profile_(profile),
      web_contents_(web_contents->GetWeakPtr()),
      enable_always_(enable_always) {}

AccessibilityLabelsBubbleModel::~AccessibilityLabelsBubbleModel() = default;

std::u16string AccessibilityLabelsBubbleModel::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_DIALOG_TITLE);
}

std::u16string AccessibilityLabelsBubbleModel::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      enable_always_
          ? IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_BUBBLE_TEXT
          : IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_BUBBLE_TEXT_ONCE);
}

std::u16string AccessibilityLabelsBubbleModel::GetButtonLabel(
    ui::mojom::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      button == ui::mojom::DialogButton::kOk
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
  if (auto* const web_contents = web_contents_.get(); web_contents) {
    AccessibilityLabelsServiceFactory::GetForProfile(profile_)
        ->EnableLabelsServiceOnce(web_contents);
  }
}

void AccessibilityLabelsBubbleModel::Cancel() {
  if (enable_always_)
    SetPref(false);
  // If not enable_always_, canceling does not have any impact.
  RecordModalDialogAccepted(/* not accepted */ false);
}

std::u16string AccessibilityLabelsBubbleModel::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL AccessibilityLabelsBubbleModel::GetHelpPageURL() const {
  return GURL(chrome::kAccessibilityLabelsLearnMoreURL);
}

void AccessibilityLabelsBubbleModel::OpenHelpPage() {
  OpenURLParams params(GetHelpPageURL(), Referrer(),
                       WindowOpenDisposition::NEW_FOREGROUND_TAB,
                       ui::PAGE_TRANSITION_LINK, false);
  if (web_contents_) {
    web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
    return;
  }
  // The web contents used to open this dialog have been destroyed.
  Browser* browser = chrome::ScopedTabbedBrowserDisplayer(profile_).browser();
  browser->OpenURL(params, /*navigation_handle_callback=*/{});
}

void AccessibilityLabelsBubbleModel::SetPref(bool enabled) {
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kAccessibilityImageLabelsEnabled, enabled);
}
