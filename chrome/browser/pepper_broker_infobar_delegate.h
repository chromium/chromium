// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PEPPER_BROKER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PEPPER_BROKER_INFOBAR_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

class HostContentSettingsMap;
class InfoBarService;
class TabSpecificContentSettings;

// Shows an infobar that asks the user whether a Pepper plugin is allowed
// to connect to its (privileged) broker. The user decision is made "sticky"
// by storing a content setting for the site.
class PepperBrokerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a pepper broker infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const GURL& url,
                     const base::string16& plugin_name,
                     HostContentSettingsMap* content_settings,
                     TabSpecificContentSettings* tab_content_settings,
                     base::OnceCallback<void(bool)> callback);
  ~PepperBrokerInfoBarDelegate() override;

 private:
  PepperBrokerInfoBarDelegate(const GURL& url,
                              const base::string16& plugin_name,
                              HostContentSettingsMap* content_settings,
                              TabSpecificContentSettings* tab_content_settings,
                              base::OnceCallback<void(bool)> callback);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;

  void DispatchCallback(bool result);

  const GURL url_;
  const base::string16 plugin_name_;
  HostContentSettingsMap* content_settings_;
  TabSpecificContentSettings* tab_content_settings_;
  base::OnceCallback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(PepperBrokerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PEPPER_BROKER_INFOBAR_DELEGATE_H_
