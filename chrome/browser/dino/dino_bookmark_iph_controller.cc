// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_bookmark_iph_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dino/dino_features.h"
#include "chrome/browser/dino/dino_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace dino {

namespace {

bool ArePromotionsEnabled() {
  if (!g_browser_process) {
    return true;
  }
  PrefService* local_state = g_browser_process->local_state();
  return !local_state || local_state->GetBoolean(::prefs::kPromotionsEnabled);
}

}  // namespace

bool CanShowDinoBookmarkPrompt(content::WebContents* web_contents) {
  // FeaturePromoController does not enforce `PromotionsEnabled` on Toast IPHs,
  // so check the enterprise policy explicitly here.
  if (!ArePromotionsEnabled()) {
    return false;
  }

  if (!web_contents ||
      web_contents->GetVisibility() != content::Visibility::VISIBLE) {
    return false;
  }

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry || entry->GetPageType() != content::PAGE_TYPE_ERROR ||
      entry->GetURL().SchemeIs(content::kChromeUIScheme)) {
    return false;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile || profile->IsOffTheRecord()) {
    return false;
  }

  PrefService* prefs = profile->GetPrefs();
  if (!prefs || !prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled)) {
    return false;
  }

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  if (!bookmark_model || !bookmark_model->loaded() ||
      bookmark_model->IsBookmarked(GURL(kDinoUrl))) {
    return false;
  }

  return true;
}

void MaybeTriggerDinoBookmarkPrompt(content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(kDinoDelight)) {
    return;
  }

  if (!CanShowDinoBookmarkPrompt(web_contents)) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  if (prefs->GetInteger(prefs::kDinoBookmarkPromptMaxTimesShownCounter) >=
      kDinoBookmarkPromptMaxTimesShown) {
    return;
  }

  auto* const user_education =
      BrowserUserEducationInterface::MaybeGetForWebContentsInTab(web_contents);
  if (!user_education) {
    return;
  }

  user_education::FeaturePromoParams params(
      feature_engagement::kIPHDelightDinoBookmarkFeature);
  params.show_promo_result_callback = base::BindOnce(
      [](base::WeakPtr<Profile> weak_profile,
         user_education::FeaturePromoResult result) {
        if (!weak_profile ||
            result != user_education::FeaturePromoResult::Success()) {
          return;
        }
        if (PrefService* prefs = weak_profile->GetPrefs()) {
          prefs->SetInteger(
              prefs::kDinoBookmarkPromptMaxTimesShownCounter,
              prefs->GetInteger(
                  prefs::kDinoBookmarkPromptMaxTimesShownCounter) +
                  1);
        }
      },
      profile->GetWeakPtr());
  user_education->MaybeShowFeaturePromo(std::move(params));
}

bool SaveDinoBookmarkFromPromo(Profile* profile) {
  if (!base::FeatureList::IsEnabled(kDinoDelight)) {
    return false;
  }

  if (!profile || profile->IsOffTheRecord()) {
    return false;
  }

  PrefService* prefs = profile->GetPrefs();
  if (!prefs || !prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled)) {
    return false;
  }

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  if (!bookmark_model || !bookmark_model->loaded()) {
    return false;
  }

  const GURL dino_url(kDinoUrl);
  if (bookmark_model->IsBookmarked(dino_url)) {
    return false;
  }

  const bookmarks::BookmarkNode* parent =
      bookmark_model->account_bookmark_bar_node()
          ? bookmark_model->account_bookmark_bar_node()
          : bookmark_model->bookmark_bar_node();
  if (!parent) {
    return false;
  }

  bookmark_model->AddNewURL(
      parent, parent->children().size(),
      l10n_util::GetStringUTF16(IDS_DELIGHT_DINO_BOOKMARK_TITLE), dino_url);
  return true;
}

bool MaybeBookmarkDinoFromIph(BrowserWindowInterface* browser) {
  if (!base::FeatureList::IsEnabled(kDinoDelight) || !browser) {
    return false;
  }

  auto* const user_education = BrowserUserEducationInterface::From(browser);
  if (!user_education ||
      !user_education->IsFeaturePromoActive(
          feature_engagement::kIPHDelightDinoBookmarkFeature)) {
    return false;
  }

  user_education->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHDelightDinoBookmarkFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  return SaveDinoBookmarkFromPromo(browser->GetProfile());
}

}  // namespace dino
