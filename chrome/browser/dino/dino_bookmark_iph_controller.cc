// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_bookmark_iph_controller.h"

#include "base/feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/dino/dino_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/grit/branded_strings.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace dino {

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
