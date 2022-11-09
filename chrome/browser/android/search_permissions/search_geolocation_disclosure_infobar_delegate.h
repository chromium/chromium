// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

class PrefService;

namespace content {
class WebContents;
}

class SearchGeolocationDisclosureInfoBarDelegate
    : public infobars::InfoBarDelegate {
 public:
  SearchGeolocationDisclosureInfoBarDelegate(
      const SearchGeolocationDisclosureInfoBarDelegate&) = delete;
  SearchGeolocationDisclosureInfoBarDelegate& operator=(
      const SearchGeolocationDisclosureInfoBarDelegate&) = delete;

  ~SearchGeolocationDisclosureInfoBarDelegate() override;

  // Create and show the infobar.
  static void Create(content::WebContents* web_contents,
                     const GURL& search_url,
                     const std::u16string& search_engine_name);

  // Determine if there is a search geolocation disclosure infobar already open.
  static bool IsSearchGeolocationDisclosureOpen(
      content::WebContents* web_contents);

  void RecordSettingsClicked();

  // The translated text of the message to display.
  const std::u16string& message_text() const { return message_text_; }

  // The range of the message that should be a link.
  const gfx::Range& inline_link_range() const { return inline_link_range_; }

  // The search URL that caused this infobar to be displayed.
  const GURL& search_url() const { return search_url_; }

 private:
  enum class DisclosureResult;

  explicit SearchGeolocationDisclosureInfoBarDelegate(
      content::WebContents* web_contents,
      const GURL& search_url,
      const std::u16string& search_engine_name);

  // InfoBarDelegate:
  void InfoBarDismissed() override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;

  // The translated text of the message to display.
  std::u16string message_text_;

  // The range of the message that should be a link.
  gfx::Range inline_link_range_;

  // The search URL that caused this infobar to be displayed.
  GURL search_url_;

  // The pref service to record prefs in.
  raw_ptr<PrefService> pref_service_;

  // The result of showing the disclosure.
  DisclosureResult result_;

  // The time the infobar was created.
  base::Time creation_time_;
};

#endif  // CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE_H_
