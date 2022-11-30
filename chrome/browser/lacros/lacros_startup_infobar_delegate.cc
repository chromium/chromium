// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_startup_infobar_delegate.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

constexpr base::StringPiece kLearnMoreURLPublic(
    "https://support.google.com/chromebook?p=chromeos-dev");
constexpr base::StringPiece kLearnMoreURLGoogleInternal(
    "https://goto.google.com/lacros-learn-more");

// Returns the single main profile, or nullptr if none is found.
Profile* GetMainProfile() {
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  const auto main_it = base::ranges::find_if(profiles, &Profile::IsMainProfile);
  if (main_it == profiles.end())
    return nullptr;
  return *main_it;
}

// Returns true if the main profile is associated with a google internal
// account.
bool IsGoogleInternal() {
  Profile* profile = GetMainProfile();
  if (!profile)
    return false;
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)
    return false;
  std::string email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  return gaia::IsGoogleInternalAccountEmail(email);
}

// Returns true if the main profile is managed.
bool IsManaged() {
  Profile* profile = GetMainProfile();
  if (!profile)
    return false;
  return profile->GetProfilePolicyConnector()->IsManaged();
}

// Returns true if Lacros is the primary browser, with ash as secondary.
bool IsPrimary() {
  return chromeos::BrowserParamsProxy::Get()->StandaloneBrowserIsPrimary();
}

}  // namespace

LacrosStartupInfoBarDelegate::LacrosStartupInfoBarDelegate() = default;
LacrosStartupInfoBarDelegate::~LacrosStartupInfoBarDelegate() = default;

// static
void LacrosStartupInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::make_unique<LacrosStartupInfoBarDelegate>()));
}

infobars::InfoBarDelegate::InfoBarIdentifier
LacrosStartupInfoBarDelegate::GetIdentifier() const {
  return infobars::InfoBarDelegate::EXPERIMENTAL_INFOBAR_DELEGATE_LACROS;
}

std::u16string LacrosStartupInfoBarDelegate::GetLinkText() const {
  // Don't show a "Learn More" link for managed, non-google enterprise accounts.
  if (IsManaged() && !IsGoogleInternal())
    return std::u16string();
  return l10n_util::GetStringUTF16(IDS_EXPERIMENTAL_LACROS_WARNING_LEARN_MORE);
}

GURL LacrosStartupInfoBarDelegate::GetLinkURL() const {
  if (IsGoogleInternal()) {
    return GURL(kLearnMoreURLGoogleInternal);
  } else {
    return GURL(kLearnMoreURLPublic);
  }
}

bool LacrosStartupInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // We must not expire the info bar, since otherwise an automated navigation
  // [which can happen at launch] will cause the info bar to disappear.
  return false;
}

bool LacrosStartupInfoBarDelegate::ShouldAnimate() const {
  return false;
}

std::u16string LacrosStartupInfoBarDelegate::GetMessageText() const {
  std::u16string base =
      l10n_util::GetStringUTF16(IDS_EXPERIMENTAL_LACROS_WARNING_MESSAGE);

  // This code relies on the assumption that the language is LTR. This is okay
  // since this message is only shown while Lacros is in development, which
  // implies that it's not ready for a larger release.
  if (IsPrimary()) {
    base += u" ";
    base += l10n_util::GetStringUTF16(
        IDS_EXPERIMENTAL_LACROS_WARNING_MESSAGE_PRIMARY);
  }

  if (IsManaged() && !IsGoogleInternal()) {
    base += u" ";
    base += l10n_util::GetStringUTF16(
        IDS_EXPERIMENTAL_LACROS_WARNING_MESSAGE_MANAGED);
  }

  return base;
}

int LacrosStartupInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}
