// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/in_product_help/in_product_help.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/variations/variations_associated_data.h"

namespace {

constexpr int kDefaultIncognitoWindowPromoShowTimeInHours = 2;
constexpr char kIncognitoWindowObservedSessionTimeKey[] =
    "incognito_window_in_product_help_observed_session_time_key";

// Note: May return null.
BrowserAppMenuButton* GetAppMenuButton() {
  auto* browser = BrowserView::GetBrowserViewForBrowser(
      BrowserList::GetInstance()->GetLastActive());
  DCHECK(browser);
  DCHECK(browser->IsActive());
  DCHECK(browser->toolbar());
  return browser->toolbar()->app_menu_button();
}

int GetPromoStringSpecifier() {
  static constexpr int kTextIds[] = {
      IDS_INCOGNITOWINDOW_PROMO_0, IDS_INCOGNITOWINDOW_PROMO_1,
      IDS_INCOGNITOWINDOW_PROMO_2, IDS_INCOGNITOWINDOW_PROMO_3};
  const std::string& str = variations::GetVariationParamValue(
      "IncognitoWindowInProductHelp", "x_promo_string");
  size_t text_specifier;
  if (!base::StringToSizeT(str, &text_specifier) ||
      text_specifier >= base::size(kTextIds)) {
    text_specifier = 0;
  }

  return kTextIds[text_specifier];
}

}  // namespace

namespace feature_engagement {

IncognitoWindowTracker::IncognitoWindowTracker(Profile* profile)
    : FeatureTracker(profile,
                     &kIPHIncognitoWindowFeature,
                     kIncognitoWindowObservedSessionTimeKey,
                     base::TimeDelta::FromHours(
                         kDefaultIncognitoWindowPromoShowTimeInHours)),
      incognito_promo_observer_(this) {}

IncognitoWindowTracker::~IncognitoWindowTracker() = default;

void IncognitoWindowTracker::OnIncognitoWindowOpened() {
  GetTracker()->NotifyEvent(events::kIncognitoWindowOpened);
}

void IncognitoWindowTracker::OnBrowsingDataCleared() {
  auto* app_menu_button = GetAppMenuButton();
  if (!app_menu_button)
    return;

  const auto severity = app_menu_button->severity();
  if (severity == AppMenuIconController::Severity::NONE && ShouldShowPromo())
    ShowPromo();
}

void IncognitoWindowTracker::OnPromoClosed() {
  GetTracker()->Dismissed(kIPHIncognitoWindowFeature);
}

void IncognitoWindowTracker::OnSessionTimeMet() {
  GetTracker()->NotifyEvent(events::kIncognitoWindowSessionTimeMet);
}

void IncognitoWindowTracker::ShowPromo() {
  DCHECK(!incognito_promo_);
  auto* app_menu_button = GetAppMenuButton();

  // Owned by its native widget. Will be destroyed when its widget is destroyed.
  incognito_promo_ = FeaturePromoBubbleView::CreateOwned(
      app_menu_button, views::BubbleBorder::TOP_RIGHT,
      FeaturePromoBubbleView::ActivationAction::ACTIVATE,
      GetPromoStringSpecifier());
  views::Widget* widget = incognito_promo_->GetWidget();
  incognito_promo_observer_.Add(widget);
  app_menu_button->SetPromoFeature(InProductHelpFeature::kIncognitoWindow);
}

void IncognitoWindowTracker::OnWidgetDestroying(views::Widget* widget) {
  OnPromoClosed();

  if (incognito_promo_observer_.IsObserving(widget)) {
    incognito_promo_observer_.Remove(widget);
    BrowserAppMenuButton* app_menu_button = GetAppMenuButton();
    if (app_menu_button)
      app_menu_button->SetPromoFeature(base::nullopt);
  }
}

}  // namespace feature_engagement
