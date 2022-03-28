// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/supported_links_infobar_delegate.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace apps {

// static
void SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
    content::WebContents* web_contents,
    const std::string& app_id) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  AppServiceProxy* proxy = AppServiceProxyFactory::GetForProfile(profile);
  if (!proxy) {
    return;
  }

  if (proxy->PreferredApps().IsPreferredAppForSupportedLinks(app_id)) {
    return;
  }

  // TODO(crbug.com/1293173): Track the number of times the infobar has been
  // shown per app, and do not show it once it has been ignored a particular
  // number of times.

  infobars::ContentInfoBarManager::FromWebContents(web_contents)
      ->AddInfoBar(CreateConfirmInfoBar(
          std::make_unique<SupportedLinksInfoBarDelegate>(profile, app_id)));
}

SupportedLinksInfoBarDelegate::SupportedLinksInfoBarDelegate(
    Profile* profile,
    const std::string& app_id)
    : profile_(profile), app_id_(app_id) {}

infobars::InfoBarDelegate::InfoBarIdentifier
SupportedLinksInfoBarDelegate::GetIdentifier() const {
  return infobars::InfoBarDelegate::InfoBarIdentifier::
      SUPPORTED_LINKS_INFOBAR_DELEGATE_CHROMEOS;
}

std::u16string SupportedLinksInfoBarDelegate::GetMessageText() const {
  std::string name;

  AppServiceProxy* proxy = AppServiceProxyFactory::GetForProfile(profile_);
  bool found = proxy->AppRegistryCache().ForOneApp(
      app_id_, [&name](const AppUpdate& app) { name = app.Name(); });
  DCHECK(found);

  return l10n_util::GetStringFUTF16(
      IDR_INTENT_PICKER_SUPPORTED_LINKS_INFOBAR_MESSAGE,
      base::UTF8ToUTF16(name));
}

std::u16string SupportedLinksInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return l10n_util::GetStringUTF16(
        IDR_INTENT_PICKER_SUPPORTED_LINKS_INFOBAR_OK_LABEL);
  }
  return l10n_util::GetStringUTF16(IDS_NO_THANKS);
}

const gfx::VectorIcon& SupportedLinksInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kSettingsIcon;
}

bool SupportedLinksInfoBarDelegate::Accept() {
  AppServiceProxy* proxy = AppServiceProxyFactory::GetForProfile(profile_);
  proxy->SetSupportedLinksPreference(app_id_);
  return true;
}

bool SupportedLinksInfoBarDelegate::Cancel() {
  // TODO(crbug.com/1293173): Update preference so that the infobar is not shown
  // again for this app.
  return true;
}

}  // namespace apps
