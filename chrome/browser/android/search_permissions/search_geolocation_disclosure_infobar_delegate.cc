// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_infobar_delegate.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/infobars/search_geolocation_disclosure_infobar.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// This enum is used in histograms, and is thus append only. Do not remove or
// re-order items.
enum class SearchGeolocationDisclosureInfoBarDelegate::DisclosureResult {
  IGNORED = 0,
  SETTINGS_CLICKED,
  DISMISSED,
  COUNT,
};

SearchGeolocationDisclosureInfoBarDelegate::
    ~SearchGeolocationDisclosureInfoBarDelegate() {
  UMA_HISTOGRAM_ENUMERATION(
      "GeolocationDisclosure.DisclosureResult",
      static_cast<base::HistogramBase::Sample>(result_),
      static_cast<base::HistogramBase::Sample>(DisclosureResult::COUNT));
  UMA_HISTOGRAM_MEDIUM_TIMES("GeolocationDisclosure.InfoBarVisibleTime",
                             base::Time::Now() - creation_time_);
}

// static
void SearchGeolocationDisclosureInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const GURL& search_url,
    const std::u16string& search_engine_name) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  // Add the new delegate.
  infobar_service->AddInfoBar(
      std::make_unique<SearchGeolocationDisclosureInfoBar>(
          base::WrapUnique(new SearchGeolocationDisclosureInfoBarDelegate(
              web_contents, search_url, search_engine_name))));
}

// static
bool SearchGeolocationDisclosureInfoBarDelegate::
    IsSearchGeolocationDisclosureOpen(content::WebContents* web_contents) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    infobars::InfoBar* existing_infobar = infobar_service->infobar_at(i);
    if (existing_infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::
            SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE_ANDROID) {
      return true;
    }
  }

  return false;
}

void SearchGeolocationDisclosureInfoBarDelegate::RecordSettingsClicked() {
  result_ = DisclosureResult::SETTINGS_CLICKED;
  // This counts as a dismissed so the dialog isn't shown again.
  pref_service_->SetBoolean(prefs::kSearchGeolocationDisclosureDismissed, true);
}

SearchGeolocationDisclosureInfoBarDelegate::
    SearchGeolocationDisclosureInfoBarDelegate(
        content::WebContents* web_contents,
        const GURL& search_url,
        const std::u16string& search_engine_name)
    : infobars::InfoBarDelegate(),
      search_url_(search_url),
      result_(DisclosureResult::IGNORED),
      creation_time_(base::Time::Now()) {
  pref_service_ = Profile::FromBrowserContext(web_contents->GetBrowserContext())
                      ->GetPrefs();
  std::u16string link = l10n_util::GetStringUTF16(
      IDS_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_SETTINGS_LINK_TEXT);
  std::vector<size_t> offsets;
  message_text_ =
      l10n_util::GetStringFUTF16(IDS_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_TEXT,
                                 search_engine_name, link, &offsets);
  inline_link_range_ = gfx::Range(offsets[1], offsets[1] + link.length());
}

void SearchGeolocationDisclosureInfoBarDelegate::InfoBarDismissed() {
  result_ = DisclosureResult::DISMISSED;
  pref_service_->SetBoolean(prefs::kSearchGeolocationDisclosureDismissed, true);
}

infobars::InfoBarDelegate::InfoBarIdentifier
SearchGeolocationDisclosureInfoBarDelegate::GetIdentifier() const {
  return SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE_ANDROID;
}

int SearchGeolocationDisclosureInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_GEOLOCATION;
}
