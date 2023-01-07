// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"

#include <memory>
#include <utility>

#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/urls.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar.h"
#endif

KnownInterceptionDisclosureCooldown*
KnownInterceptionDisclosureCooldown::GetInstance() {
  return base::Singleton<KnownInterceptionDisclosureCooldown>::get();
}

bool KnownInterceptionDisclosureCooldown::IsActive(Profile* profile) {
  base::Time last_dismissal;

#if BUILDFLAG(IS_ANDROID)
  last_dismissal = profile->GetPrefs()->GetTime(
      prefs::kKnownInterceptionDisclosureInfobarLastShown);
#else
  last_dismissal = last_dismissal_time_;
#endif

  // Suppress the disclosure UI for 7 days after showing it to the user.
  return (clock_->Now() - last_dismissal) <= base::Days(7);
}

void KnownInterceptionDisclosureCooldown::Activate(Profile* profile) {
#if BUILDFLAG(IS_ANDROID)
  profile->GetPrefs()->SetTime(
      prefs::kKnownInterceptionDisclosureInfobarLastShown, clock_->Now());
#else
  last_dismissal_time_ = clock_->Now();
#endif
}

void KnownInterceptionDisclosureCooldown::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

KnownInterceptionDisclosureCooldown::KnownInterceptionDisclosureCooldown() =
    default;

KnownInterceptionDisclosureCooldown::~KnownInterceptionDisclosureCooldown() =
    default;

void MaybeShowKnownInterceptionDisclosureDialog(
    content::WebContents* web_contents,
    net::CertStatus cert_status) {
  auto* disclosure_tracker = KnownInterceptionDisclosureCooldown::GetInstance();
  if (!(cert_status & net::CERT_STATUS_KNOWN_INTERCEPTION_DETECTED) &&
      !disclosure_tracker->get_has_seen_known_interception()) {
    return;
  }

  disclosure_tracker->set_has_seen_known_interception(true);

  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto delegate =
      std::make_unique<KnownInterceptionDisclosureInfoBarDelegate>(profile);

  if (!KnownInterceptionDisclosureCooldown::GetInstance()->IsActive(profile)) {
#if BUILDFLAG(IS_ANDROID)
    infobar_manager->AddInfoBar(
        KnownInterceptionDisclosureInfoBar::CreateInfoBar(std::move(delegate)));
#else
    infobar_manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));
#endif
  }
}

KnownInterceptionDisclosureInfoBarDelegate::
    KnownInterceptionDisclosureInfoBarDelegate(Profile* profile)
    : profile_(profile) {}

infobars::InfoBarDelegate::InfoBarIdentifier
KnownInterceptionDisclosureInfoBarDelegate::GetIdentifier() const {
  return KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE;
}

std::u16string KnownInterceptionDisclosureInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL KnownInterceptionDisclosureInfoBarDelegate::GetLinkURL() const {
  return GURL("chrome://connection-monitoring-detected/");
}

bool KnownInterceptionDisclosureInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

void KnownInterceptionDisclosureInfoBarDelegate::InfoBarDismissed() {
  KnownInterceptionDisclosureCooldown::GetInstance()->Activate(profile_);
  Cancel();
}

std::u16string KnownInterceptionDisclosureInfoBarDelegate::GetMessageText()
    const {
  return l10n_util::GetStringUTF16(IDS_KNOWN_INTERCEPTION_HEADER);
}

int KnownInterceptionDisclosureInfoBarDelegate::GetButtons() const {
#if BUILDFLAG(IS_ANDROID)
  return BUTTON_OK;
#else
  return BUTTON_NONE;
#endif
}

bool KnownInterceptionDisclosureInfoBarDelegate::Accept() {
  KnownInterceptionDisclosureCooldown::GetInstance()->Activate(profile_);
  return true;
}

// Platform specific implementations.
#if BUILDFLAG(IS_ANDROID)
int KnownInterceptionDisclosureInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_WARNING;
}

std::u16string KnownInterceptionDisclosureInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_KNOWN_INTERCEPTION_INFOBAR_BUTTON_TEXT);
}

std::u16string KnownInterceptionDisclosureInfoBarDelegate::GetDescriptionText()
    const {
  return l10n_util::GetStringUTF16(IDS_KNOWN_INTERCEPTION_BODY1);
}

// static
void KnownInterceptionDisclosureInfoBarDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(
      prefs::kKnownInterceptionDisclosureInfobarLastShown, base::Time());
}
#endif
