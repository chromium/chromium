// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pepper_broker_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"

// static
void PepperBrokerInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const GURL& url,
    const base::string16& plugin_name,
    HostContentSettingsMap* content_settings,
    TabSpecificContentSettings* tab_content_settings,
    base::OnceCallback<void(bool)> callback) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      base::WrapUnique(new PepperBrokerInfoBarDelegate(
          url, plugin_name, content_settings, tab_content_settings,
          std::move(callback)))));
}

PepperBrokerInfoBarDelegate::PepperBrokerInfoBarDelegate(
    const GURL& url,
    const base::string16& plugin_name,
    HostContentSettingsMap* content_settings,
    TabSpecificContentSettings* tab_content_settings,
    base::OnceCallback<void(bool)> callback)
    : ConfirmInfoBarDelegate(),
      url_(url),
      plugin_name_(plugin_name),
      content_settings_(content_settings),
      tab_content_settings_(tab_content_settings),
      callback_(std::move(callback)) {}

PepperBrokerInfoBarDelegate::~PepperBrokerInfoBarDelegate() {
  if (!callback_.is_null())
    std::move(callback_).Run(false);
}

infobars::InfoBarDelegate::InfoBarIdentifier
PepperBrokerInfoBarDelegate::GetIdentifier() const {
  return PEPPER_BROKER_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& PepperBrokerInfoBarDelegate::GetVectorIcon() const {
  return kExtensionIcon;
}

base::string16 PepperBrokerInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_PEPPER_BROKER_MESSAGE, plugin_name_,
      url_formatter::FormatUrlForSecurityDisplay(url_));
}

base::string16 PepperBrokerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PEPPER_BROKER_ALLOW_BUTTON : IDS_PEPPER_BROKER_DENY_BUTTON);
}

bool PepperBrokerInfoBarDelegate::Accept() {
  DispatchCallback(true);
  return true;
}

bool PepperBrokerInfoBarDelegate::Cancel() {
  DispatchCallback(false);
  return true;
}

base::string16 PepperBrokerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL PepperBrokerInfoBarDelegate::GetLinkURL() const {
  return GURL("https://support.google.com/chrome/?p=ib_pepper_broker");
}

void PepperBrokerInfoBarDelegate::DispatchCallback(bool result) {
  base::RecordAction(
      result ? base::UserMetricsAction("PPAPI.BrokerInfobarClickedAllow")
             : base::UserMetricsAction("PPAPI.BrokerInfobarClickedDeny"));
  std::move(callback_).Run(result);
  content_settings_->SetContentSettingDefaultScope(
      url_, GURL(), ContentSettingsType::PPAPI_BROKER, std::string(),
      result ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
  tab_content_settings_->SetPepperBrokerAllowed(result);
}
