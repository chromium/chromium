// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_ambient_badge_infobar_delegate.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/installable_ambient_badge_infobar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

InstallableAmbientBadgeInfoBarDelegate::
    ~InstallableAmbientBadgeInfoBarDelegate() = default;

// static
void InstallableAmbientBadgeInfoBarDelegate::Create(
    content::WebContents* web_contents,
    base::WeakPtr<Client> weak_client,
    const base::string16& app_name,
    const SkBitmap& primary_icon,
    const bool is_primary_icon_maskable,
    const GURL& start_url) {
  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(std::make_unique<InstallableAmbientBadgeInfoBar>(
          std::unique_ptr<InstallableAmbientBadgeInfoBarDelegate>(
              new InstallableAmbientBadgeInfoBarDelegate(
                  weak_client, app_name, primary_icon, is_primary_icon_maskable,
                  start_url))));
}

void InstallableAmbientBadgeInfoBarDelegate::AddToHomescreen() {
  if (!weak_client_.get())
    return;

  weak_client_->AddToHomescreenFromBadge();
}

const base::string16 InstallableAmbientBadgeInfoBarDelegate::GetMessageText()
    const {
  if (!base::FeatureList::IsEnabled(features::kAddToHomescreenMessaging))
    return l10n_util::GetStringFUTF16(IDS_AMBIENT_BADGE_INSTALL, app_name_);

  bool include_no_download_required = base::GetFieldTrialParamByFeatureAsBool(
      features::kAddToHomescreenMessaging, "include_no_download_required",
      /* default_value= */ false);

  return l10n_util::GetStringFUTF16(
      include_no_download_required
          ? IDS_AMBIENT_BADGE_INSTALL_ALTERNATIVE_NO_DOWNLOAD_REQUIRED
          : IDS_AMBIENT_BADGE_INSTALL_ALTERNATIVE,
      app_name_);
}

const SkBitmap& InstallableAmbientBadgeInfoBarDelegate::GetPrimaryIcon() const {
  return primary_icon_;
}

bool InstallableAmbientBadgeInfoBarDelegate::GetIsPrimaryIconMaskable() const {
  return is_primary_icon_maskable_;
}

InstallableAmbientBadgeInfoBarDelegate::InstallableAmbientBadgeInfoBarDelegate(
    base::WeakPtr<Client> weak_client,
    const base::string16& app_name,
    const SkBitmap& primary_icon,
    const bool is_primary_icon_maskable,
    const GURL& start_url)
    : infobars::InfoBarDelegate(),
      weak_client_(weak_client),
      app_name_(app_name),
      primary_icon_(primary_icon),
      is_primary_icon_maskable_(is_primary_icon_maskable),
      start_url_(start_url) {}

infobars::InfoBarDelegate::InfoBarIdentifier
InstallableAmbientBadgeInfoBarDelegate::GetIdentifier() const {
  return INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE;
}

void InstallableAmbientBadgeInfoBarDelegate::InfoBarDismissed() {
  if (!weak_client_.get())
    return;

  weak_client_->BadgeDismissed();
}
