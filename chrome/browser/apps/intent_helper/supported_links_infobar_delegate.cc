// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/supported_links_infobar_delegate.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#include "chrome/browser/apps/intent_helper/supported_links_infobar_prefs_service.h"
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace apps {

// static
void SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
    content::WebContents* web_contents,
    const std::string& app_id) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (!IsSetSupportedLinksPreferenceSupported()) {
    return;
  }

  AppServiceProxy* proxy = AppServiceProxyFactory::GetForProfile(profile);
  if (!proxy) {
    return;
  }

  if (proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_id)) {
    return;
  }

  auto* prefs_service = SupportedLinksInfoBarPrefsService::Get(profile);
  if (!prefs_service || prefs_service->ShouldHideInfoBarForApp(app_id)) {
    return;
  }

  infobars::ContentInfoBarManager::FromWebContents(web_contents)
      ->AddInfoBar(CreateConfirmInfoBar(
          std::make_unique<SupportedLinksInfoBarDelegate>(profile, app_id)));
}

// static
void SupportedLinksInfoBarDelegate::RemoveSupportedLinksInfoBar(
    content::WebContents* web_contents) {
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  for (size_t i = 0; i < infobar_manager->infobar_count(); i++) {
    auto* infobar = infobar_manager->infobar_at(i);
    if (infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::SUPPORTED_LINKS_INFOBAR_DELEGATE_CHROMEOS) {
      // There should only ever be one supported links infobar visible at a
      // time, so we can just remove it and exit.
      infobar_manager->RemoveInfoBar(infobar);
      return;
    }
  }
}

// static
bool SupportedLinksInfoBarDelegate::IsSetSupportedLinksPreferenceSupported() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return (chromeos::LacrosService::Get()->GetInterfaceVersion(
              crosapi::mojom::AppServiceProxy::Uuid_) >=
          static_cast<int>(crosapi::mojom::AppServiceProxy::MethodMinVersions::
                               kSetSupportedLinksPreferenceMinVersion));
#else
  return true;
#endif
}

SupportedLinksInfoBarDelegate::SupportedLinksInfoBarDelegate(
    Profile* profile,
    const std::string& app_id)
    : profile_(profile), app_id_(app_id) {}

SupportedLinksInfoBarDelegate::~SupportedLinksInfoBarDelegate() {
  if (!action_taken_) {
    SupportedLinksInfoBarPrefsService::Get(profile_)->MarkInfoBarIgnored(
        app_id_);
  }
}

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

bool SupportedLinksInfoBarDelegate::IsCloseable() const {
  return false;
}

bool SupportedLinksInfoBarDelegate::Accept() {
  action_taken_ = true;
  AppServiceProxy* proxy = AppServiceProxyFactory::GetForProfile(profile_);
  proxy->SetSupportedLinksPreference(app_id_);

  // The support links infobar only shows for webapps so we record the metrics
  // event under kWeb.
  IntentHandlingMetrics::RecordLinkCapturingEvent(
      PickerEntryType::kWeb,
      IntentHandlingMetrics::LinkCapturingEvent::kSettingsChanged);
  return true;
}

bool SupportedLinksInfoBarDelegate::Cancel() {
  action_taken_ = true;
  SupportedLinksInfoBarPrefsService::Get(profile_)->MarkInfoBarDismissed(
      app_id_);
  return true;
}

}  // namespace apps
