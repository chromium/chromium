// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spelling_bubble_model.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

SpellingBubbleModel::SpellingBubbleModel(Profile* profile,
                                         WebContents* web_contents)
    : profile_(profile), web_contents_(web_contents->GetWeakPtr()) {}

SpellingBubbleModel::~SpellingBubbleModel() = default;

std::u16string SpellingBubbleModel::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_TITLE);
}

std::u16string SpellingBubbleModel::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_TEXT);
}

std::u16string SpellingBubbleModel::GetButtonLabel(
    ui::mojom::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      button == ui::mojom::DialogButton::kOk
          ? IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_ENABLE
          : IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_DISABLE);
}

void SpellingBubbleModel::Accept() {
  SetPref(true);
}

void SpellingBubbleModel::Cancel() {
  SetPref(false);
}

std::u16string SpellingBubbleModel::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL SpellingBubbleModel::GetHelpPageURL() const {
  return GURL(chrome::kPrivacyLearnMoreURL);
}

void SpellingBubbleModel::OpenHelpPage() {
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

void SpellingBubbleModel::SetPref(bool enabled) {
  PrefService* pref = profile_->GetPrefs();
  DCHECK(pref);
  pref->SetBoolean(spellcheck::prefs::kSpellCheckUseSpellingService, enabled);

  // If the user has explicitly opted in to using the spelling service,
  // spellcheck should be enabled so that the spelling service can run.
  if (enabled) {
    pref->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);
  }
}
