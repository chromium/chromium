// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/enable_link_capturing_infobar_delegate.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace apps {
namespace {
void IncrementIgnoreCount(webapps::AppId app_id,
                          web_app::AppLock& app_lock,
                          base::Value::Dict& debug_result) {
  web_app::ScopedRegistryUpdate update = app_lock.sync_bridge().BeginUpdate();
  web_app::WebApp* app = update->UpdateApp(app_id);

  debug_result.Set("app_id", app_id);
  if (!app) {
    debug_result.Set("error", "AppId does not exist.");
    return;
  }
  int new_count =
      base::ClampedNumeric(app->supported_links_offer_ignore_count()) + 1;
  app->SetSupportedLinksOfferIgnoreCount(new_count);

  debug_result.Set("supported_links_offer_ignore_count", new_count);
}

void IncrementDismissCount(webapps::AppId app_id,
                           web_app::AppLock& app_lock,
                           base::Value::Dict& debug_result) {
  web_app::ScopedRegistryUpdate update = app_lock.sync_bridge().BeginUpdate();
  web_app::WebApp* app = update->UpdateApp(app_id);

  debug_result.Set("app_id", app_id);
  if (!app) {
    debug_result.Set("error", "AppId does not exist.");
    return;
  }
  int new_count =
      base::ClampedNumeric(app->supported_links_offer_dismiss_count()) + 1;
  app->SetSupportedLinksOfferDismissCount(new_count);

  debug_result.Set("supported_links_offer_dismiss_count", new_count);
}
}  // namespace

// Searches the toolbar on this web contents for this infobar, and returns it
// if found.
infobars::InfoBar* EnableLinkCapturingInfoBarDelegate::FindInfoBar(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  CHECK(infobar_manager);
  const auto it = base::ranges::find(
      infobar_manager->infobars(),
      infobars::InfoBarDelegate::ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE,
      &infobars::InfoBar::GetIdentifier);
  return it != infobar_manager->infobars().cend() ? *it : nullptr;
}

// static
std::unique_ptr<EnableLinkCapturingInfoBarDelegate>
EnableLinkCapturingInfoBarDelegate::MaybeCreate(
    content::WebContents* web_contents,
    const std::string& app_id) {
  CHECK(web_contents);

  // Check that no infobar has already been added.
  CHECK(!EnableLinkCapturingInfoBarDelegate::FindInfoBar(web_contents));

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  CHECK(profile);

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    return nullptr;
  }

  // Do not show the infobar if links are already captured.
  if (provider->registrar_unsafe().CapturesLinksInScope(app_id)) {
    return nullptr;
  }

  const GURL& url = web_contents->GetLastCommittedURL();
  if (!web_app::IsValidScopeForLinkCapturing(url)) {
    return nullptr;
  }

  // A 'outer' app can still be launched from the intent picker, even if there
  // is a more applicable 'nested' app. This is allowed, but the 'outer' app can
  // never capture links. Thus, disable this infobar in that case.
  if (!provider->registrar_unsafe().IsLinkCapturableByApp(app_id, url)) {
    return nullptr;
  }

  const web_app::WebApp* app = provider->registrar_unsafe().GetAppById(app_id);
  CHECK(app);
  constexpr int kSupportedLinksMaxIgnoreCount = 3;
  if (app->supported_links_offer_ignore_count() >=
      kSupportedLinksMaxIgnoreCount) {
    return nullptr;
  }
  constexpr int kSupportedLinksMaxDismissCount = 2;
  if (app->supported_links_offer_dismiss_count() >=
      kSupportedLinksMaxDismissCount) {
    return nullptr;
  }

  return base::WrapUnique(
      new EnableLinkCapturingInfoBarDelegate(*profile, app_id));
}

// static
void EnableLinkCapturingInfoBarDelegate::RemoveInfoBar(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  CHECK(infobar_manager);
  infobars::InfoBar* infobar =
      EnableLinkCapturingInfoBarDelegate::FindInfoBar(web_contents);
  if (infobar) {
    infobar_manager->RemoveInfoBar(infobar);
  }
}

EnableLinkCapturingInfoBarDelegate::~EnableLinkCapturingInfoBarDelegate() {
  if (action_taken_) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("LinkCapturingIgnoredFromInfoBar"));
  provider_->scheduler().ScheduleCallback(
      "IncrementSupportedLinksOfferIgnoreCount",
      web_app::AppLockDescription(app_id_),
      base::BindOnce(&IncrementIgnoreCount, app_id_),
      /*on_complete=*/base::DoNothing());
}

infobars::InfoBarDelegate::InfoBarIdentifier
EnableLinkCapturingInfoBarDelegate::GetIdentifier() const {
  return infobars::InfoBarDelegate::InfoBarIdentifier::
      ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE;
}

std::u16string EnableLinkCapturingInfoBarDelegate::GetMessageText() const {
  std::string name;
  name = provider_->registrar_unsafe().GetAppShortName(app_id_);
  return l10n_util::GetStringFUTF16(
      IDR_INTENT_PICKER_SUPPORTED_LINKS_INFOBAR_MESSAGE,
      base::UTF8ToUTF16(name));
}

std::u16string EnableLinkCapturingInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      button == BUTTON_OK ? IDR_INTENT_PICKER_SUPPORTED_LINKS_INFOBAR_OK_LABEL
                          : IDS_NO_THANKS);
}

const gfx::VectorIcon& EnableLinkCapturingInfoBarDelegate::GetVectorIcon()
    const {
  return vector_icons::kSettingsIcon;
}

bool EnableLinkCapturingInfoBarDelegate::IsCloseable() const {
  return true;
}

bool EnableLinkCapturingInfoBarDelegate::Accept() {
  action_taken_ = true;
  base::RecordAction(
      base::UserMetricsAction("LinkCapturingAcceptedFromInfoBar"));
  provider_->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
      app_id_, true, base::DoNothing());
  return true;
}

bool EnableLinkCapturingInfoBarDelegate::Cancel() {
  action_taken_ = true;
  base::RecordAction(
      base::UserMetricsAction("LinkCapturingCancelledFromInfoBar"));
  provider_->scheduler().ScheduleCallback(
      "IncrementSupportedLinksOfferDismissCount",
      web_app::AppLockDescription(app_id_),
      base::BindOnce(&IncrementDismissCount, app_id_),
      /*on_complete*/ base::DoNothing());
  return true;
}

EnableLinkCapturingInfoBarDelegate::EnableLinkCapturingInfoBarDelegate(
    Profile& profile,
    const webapps::AppId& app_id)
    : profile_(profile),
      provider_(*web_app::WebAppProvider::GetForWebApps(&profile)),
      app_id_(app_id) {}

}  // namespace apps
