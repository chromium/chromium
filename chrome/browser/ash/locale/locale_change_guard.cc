// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/locale/locale_change_guard.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using ::base::UserMetricsAction;
using ::content::WebContents;

// This is the list of languages that do not require user notification when
// locale is switched automatically between regions within the same language.
//
// New language in kAcceptLanguageList should be added either here or to
// to the exception list in unit test.
const char* const kSkipShowNotificationLanguages[4] = {"en", "de", "fr", "it"};

}  // anonymous namespace

LocaleChangeGuard::LocaleChangeGuard(Profile* profile, PrefService* local_state)
    : profile_(profile), local_state_(local_state) {
  DCHECK(profile_);
  DeviceSettingsService::Get()->AddObserver(this);
}

LocaleChangeGuard::~LocaleChangeGuard() {
  if (DeviceSettingsService::IsInitialized())
    DeviceSettingsService::Get()->RemoveObserver(this);
}

void LocaleChangeGuard::OnLogin() {
  if (session_manager::SessionManager::Get()->IsSessionStarted()) {
    Check();
  } else {
    if (session_observation_.IsObserving()) {
      DCHECK(session_observation_.IsObservingSource(
          session_manager::SessionManager::Get()));
      return;
    }
    session_observation_.Observe(session_manager::SessionManager::Get());
  }
}

void LocaleChangeGuard::RevertLocaleChange() {
  if (from_locale_.empty() || to_locale_.empty()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  if (reverted_)
    return;
  reverted_ = true;
  base::RecordAction(UserMetricsAction("LanguageChange_Revert"));
  profile_->ChangeAppLocale(from_locale_,
                            Profile::APP_LOCALE_CHANGED_VIA_REVERT);
  chrome::AttemptUserExit();
}

void LocaleChangeGuard::OnUserSessionStarted(bool is_primary_user) {
  session_observation_.Reset();
  Check();
}

void LocaleChangeGuard::OwnershipStatusChanged() {
  if (!DeviceSettingsService::Get()->HasPrivateOwnerKey()) {
    return;
  }

  if (!local_state_) {
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  std::string owner_locale =
      prefs->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&owner_locale);
  if (!owner_locale.empty()) {
    local_state_->SetString(prefs::kOwnerLocale, owner_locale);
  }
}

void LocaleChangeGuard::Check() {
  std::string cur_locale = g_browser_process->GetApplicationLocale();
  if (cur_locale.empty()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  if (prefs == nullptr) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  std::string to_locale = prefs->GetString(language::prefs::kApplicationLocale);
  // Ensure that synchronization does not change the locale to a value not
  // allowed by enterprise policy.
  if (!locale_util::IsAllowedUILanguage(to_locale, prefs)) {
    prefs->SetString(language::prefs::kApplicationLocale,
                     locale_util::GetAllowedFallbackUILanguage(prefs));
  }

  language::ConvertToActualUILocale(&to_locale);

  if (to_locale != cur_locale && locale_changed_during_login_) {
    LocaleUpdateController::Get()->OnLocaleChanged();
    return;
  }

  std::string from_locale = prefs->GetString(prefs::kApplicationLocaleBackup);

  if (!RequiresUserConfirmation(from_locale, to_locale)) {
    // If the locale changed during login (e.g. from the owner's locale), just
    // notify ash about the change, so system UI gets updated.
    // If the change also requires user confirmation, the UI will be updates as
    // part of `LocaleUpdateController::ConfirmLocaleChange`.
    if (locale_changed_during_login_)
      LocaleUpdateController::Get()->OnLocaleChanged();
    return;
  }

  // Showing notification.
  if (from_locale_ != from_locale || to_locale_ != to_locale) {
    // Falling back to showing message in current locale.
    LOG(ERROR) << "Showing locale change notification in current (not "
                  "previous) language";
    PrepareChangingLocale(from_locale, to_locale);
  }

  LocaleUpdateController::Get()->ConfirmLocaleChange(
      cur_locale, from_locale_, to_locale_,
      base::BindOnce(&LocaleChangeGuard::OnResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocaleChangeGuard::OnResult(LocaleNotificationResult result) {
  switch (result) {
    case LocaleNotificationResult::kAccept:
      AcceptLocaleChange();
      break;
    case LocaleNotificationResult::kRevert:
      RevertLocaleChange();
      break;
  }
}

void LocaleChangeGuard::AcceptLocaleChange() {
  if (from_locale_.empty() || to_locale_.empty()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // Check whether locale has been reverted or changed.
  // If not: mark current locale as accepted.
  if (reverted_)
    return;
  PrefService* prefs = profile_->GetPrefs();
  if (prefs == nullptr) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  if (prefs->GetString(language::prefs::kApplicationLocale) != to_locale_)
    return;
  base::RecordAction(UserMetricsAction("LanguageChange_Accept"));
  prefs->SetString(prefs::kApplicationLocaleBackup, to_locale_);
  prefs->SetString(prefs::kApplicationLocaleAccepted, to_locale_);
}

void LocaleChangeGuard::PrepareChangingLocale(const std::string& from_locale,
                                              const std::string& to_locale) {
  std::string cur_locale = g_browser_process->GetApplicationLocale();
  if (!from_locale.empty())
    from_locale_ = from_locale;
  if (!to_locale.empty())
    to_locale_ = to_locale;
}

bool LocaleChangeGuard::RequiresUserConfirmation(
    const std::string& from_locale,
    const std::string& to_locale) const {
  // No locale change was detected for the user.
  if (from_locale.empty() || from_locale == to_locale)
    return false;

  // The target locale is already accepted.
  if (profile_->GetPrefs()->GetString(prefs::kApplicationLocaleAccepted) ==
      to_locale) {
    return false;
  }

  return ShouldShowLocaleChangeNotification(from_locale, to_locale);
}

// static
bool LocaleChangeGuard::ShouldShowLocaleChangeNotification(
    const std::string& from_locale,
    const std::string& to_locale) {
  const std::string from_lang = l10n_util::GetLanguage(from_locale);
  const std::string to_lang = l10n_util::GetLanguage(to_locale);

  if (from_locale == to_locale)
    return false;

  if (from_lang != to_lang)
    return true;

  return !base::Contains(kSkipShowNotificationLanguages, from_lang);
}

// static
const char* const*
LocaleChangeGuard::GetSkipShowNotificationLanguagesForTesting() {
  return kSkipShowNotificationLanguages;
}

// static
size_t LocaleChangeGuard::GetSkipShowNotificationLanguagesSizeForTesting() {
  return std::size(kSkipShowNotificationLanguages);
}

}  // namespace ash
