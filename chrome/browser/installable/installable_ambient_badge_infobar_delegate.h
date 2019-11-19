// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/infobars/core/infobar_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Delegate for a infobar shown to users when they visit a progressive web app.
// Tapping the infobar triggers the add to home screen flow.
class InstallableAmbientBadgeInfoBarDelegate
    : public infobars::InfoBarDelegate {
 public:
  class Client {
   public:
    // Called to trigger the add to home screen flow.
    virtual void AddToHomescreenFromBadge() = 0;

    // Called to inform the client that the badge was dismissed.
    virtual void BadgeDismissed() = 0;

    virtual ~Client() {}
  };

  ~InstallableAmbientBadgeInfoBarDelegate() override;

  // Create and show the infobar.
  static void Create(content::WebContents* web_contents,
                     base::WeakPtr<Client> weak_client,
                     const base::string16& app_name,
                     const SkBitmap& primary_icon,
                     const bool is_primary_icon_maskable,

                     const GURL& start_url);

  void AddToHomescreen();
  const base::string16 GetMessageText() const;
  const SkBitmap& GetPrimaryIcon() const;
  bool GetIsPrimaryIconMaskable() const;
  const GURL& GetUrl() const { return start_url_; }

 private:
  InstallableAmbientBadgeInfoBarDelegate(base::WeakPtr<Client> weak_client,
                                         const base::string16& app_name,
                                         const SkBitmap& primary_icon,
                                         const bool is_primary_icon_maskable,
                                         const GURL& start_url);

  // InfoBarDelegate overrides:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  void InfoBarDismissed() override;

  base::WeakPtr<Client> weak_client_;
  const base::string16 app_name_;
  const SkBitmap primary_icon_;
  const bool is_primary_icon_maskable_;
  const GURL& start_url_;

  DISALLOW_COPY_AND_ASSIGN(InstallableAmbientBadgeInfoBarDelegate);
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE_H_
