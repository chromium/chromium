// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_INFOBAR_DELEGATE_H_

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class WebContents;
}

// This InfoBar notifies the user that Data Saver now also applies to HTTPS
// pages.
class PreviewsLitePageInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ~PreviewsLitePageInfoBarDelegate() override;

  // Actions taken on the infobar. This enum must remain synchronized with the
  // enum of the same name in metrics/histograms/enums.xml.
  enum PreviewsLitePageInfoBarAction {
    kInfoBarShown = 0,
    kInfoBarDismissed = 1,
    kInfoBarLinkClicked = 2,
    kMaxValue = kInfoBarLinkClicked,
  };

  // Shows the InfoBar.
  static void Create(content::WebContents* web_contents);

 private:
  PreviewsLitePageInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  void InfoBarDismissed() override;
  int GetButtons() const override;
  base::string16 GetMessageText() const override;
#if defined(OS_ANDROID)
  int GetIconId() const override;
  base::string16 GetLinkText() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;
#endif

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_INFOBAR_DELEGATE_H_
