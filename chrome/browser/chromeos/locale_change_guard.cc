// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/locale_change_guard.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;
using content::WebContents;

namespace chromeos {

namespace {

// This is the list of languages that do not require user notification when
// locale is switched automatically between regions within the same language.
//
// New language in kAcceptLanguageList should be added either here or to
// to the exception list in unit test.
const char* const kSkipShowNotificationLanguages[4] = {"en", "de", "fr", "it"};

}  // anonymous namespace

LocaleChangeGuard::LocaleChangeGuard(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
  DeviceSettingsService::Get()->AddObserver(this);
}

LocaleChangeGuard::~LocaleChangeGuard() {
  if (DeviceSettingsService::IsInitialized())
    DeviceSettingsService::Get()->RemoveObserver(this);
}

void LocaleChangeGuard::OnLogin() {
  session_observer_.Add(session_manager::SessionManager::Get());
  registrar_.Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void LocaleChangeGuard::RevertLocaleChange() {
  if (from_locale_.empty() || to_locale_.empty()) {
    NOTREACHED();
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

void LocaleChangeGuard::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME);
  if (profile_ != content::Source<WebContents>(source)->GetBrowserContext())
    return;

  main_frame_loaded_ = true;
  // We need to perform locale change check only once, so unsubscribe.
  registrar_.Remove(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                    content::NotificationService::AllSources());
  if (session_manager::SessionManager::Get()->IsSessionStarted())
    Check();
}

void LocaleChangeGuard::OnUserSessionStarted(bool is_primary_user) {
  session_observer_.RemoveAll();
  if (main_frame_loaded_)
    Check();
}

void LocaleChangeGuard::OwnershipStatusChanged() {
  if (!DeviceSettingsService::Get()->HasPrivateOwnerKey())
    return;
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return;
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  std::string owner_locale =
      prefs->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&owner_locale);
  if (!owner_locale.empty())
    local_state->SetString(prefs::kOwnerLocale, owner_locale);
}

void LocaleChangeGuard::Check() {
  std::string cur_locale = g_browser_process->GetApplicationLocale();
  if (cur_locale.empty()) {
    NOTREACHED();
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  if (prefs == NULL) {
    NOTREACHED();
    return;
  }

  std::string to_locale = prefs->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&to_locale);
  if (to_locale != cur_locale) {
    // This conditional branch can occur in cases like:
    // (1) kApplicationLocale preference was modified by synchronization;
    // (2) kApplicationLocale is managed by policy.

    // Ensure that synchronization does not change the locale to a value not
    // allowed by enterprise policy.
    if (!chromeos::locale_util::IsAllowedUILanguage(to_locale, prefs))
      prefs->SetString(language::prefs::kApplicationLocale, cur_locale);
    return;
  }

  std::string from_locale = prefs->GetString(prefs::kApplicationLocaleBackup);
  if (from_locale.empty() || from_locale == to_locale)
    return;  // No locale change was detected, just exit.

  if (prefs->GetString(prefs::kApplicationLocaleAccepted) == to_locale)
    return;  // Already accepted.

  // Locale change detected.
  if (!ShouldShowLocaleChangeNotification(from_locale, to_locale))
    return;

  // Showing notification.
  if (from_locale_ != from_locale || to_locale_ != to_locale) {
    // Falling back to showing message in current locale.
    LOG(ERROR) << "Showing locale change notification in current (not "
                  "previous) language";
    PrepareChangingLocale(from_locale, to_locale);
  }

  ash::LocaleUpdateController::Get()->OnLocaleChanged(
      cur_locale, from_locale_, to_locale_,
      base::Bind(&LocaleChangeGuard::OnResult, AsWeakPtr()));
}

void LocaleChangeGuard::OnResult(ash::LocaleNotificationResult result) {
  switch (result) {
    case ash::LocaleNotificationResult::kAccept:
      AcceptLocaleChange();
      break;
    case ash::LocaleNotificationResult::kRevert:
      RevertLocaleChange();
      break;
  }
}

void LocaleChangeGuard::AcceptLocaleChange() {
  if (from_locale_.empty() || to_locale_.empty()) {
    NOTREACHED();
    return;
  }

  // Check whether locale has been reverted or changed.
  // If not: mark current locale as accepted.
  if (reverted_)
    return;
  PrefService* prefs = profile_->GetPrefs();
  if (prefs == NULL) {
    NOTREACHED();
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
  return base::size(kSkipShowNotificationLanguages);
}

}  // namespace chromeos
