// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/session_startup_pref.h"

#include <stddef.h>

#include <string>

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/startup/startup_tab.h"
#endif

namespace {

// Converts a SessionStartupPref::Type to an integer written to prefs.
int TypeToPrefValue(SessionStartupPref::Type type) {
  switch (type) {
    case SessionStartupPref::LAST:
      return SessionStartupPref::kPrefValueLast;
    case SessionStartupPref::URLS:
      return SessionStartupPref::kPrefValueURLs;
    case SessionStartupPref::LAST_AND_URLS:
      return SessionStartupPref::kPrefValueLastAndURLs;
    default:
      return SessionStartupPref::kPrefValueNewTab;
  }
}

void URLListToPref(const base::Value::List& url_list,
                   SessionStartupPref* pref) {
  pref->urls.clear();
  for (const base::Value& i : url_list) {
    const std::string* url_text = i.GetIfString();
    if (url_text) {
      GURL fixed_url = url_formatter::FixupURL(*url_text, std::string());
      pref->urls.push_back(fixed_url);
    }
  }
}

}  // namespace

// static
void SessionStartupPref::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_ANDROID)
  uint32_t flags = PrefRegistry::NO_REGISTRATION_FLAGS;
#else
  uint32_t flags = user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;
#endif
  registry->RegisterIntegerPref(prefs::kRestoreOnStartup,
                                TypeToPrefValue(GetDefaultStartupType()),
                                flags);
  registry->RegisterListPref(prefs::kURLsToRestoreOnStartup, flags);
}

// static
SessionStartupPref::Type SessionStartupPref::GetDefaultStartupType() {
#if BUILDFLAG(IS_CHROMEOS)
  return SessionStartupPref::LAST;
#else
  return SessionStartupPref::DEFAULT;
#endif
}

// static
void SessionStartupPref::SetStartupPref(Profile* profile,
                                        const SessionStartupPref& pref) {
  DCHECK(profile);
  SetStartupPref(profile->GetPrefs(), pref);
}

// static
void SessionStartupPref::SetStartupPref(PrefService* prefs,
                                        const SessionStartupPref& pref) {
  DCHECK(prefs);

  if (!SessionStartupPref::TypeIsManaged(prefs))
    prefs->SetInteger(prefs::kRestoreOnStartup, TypeToPrefValue(pref.type));

  if (!SessionStartupPref::URLsAreManaged(prefs)) {
    // Always save the URLs, that way the UI can remain consistent even if the
    // user changes the startup type pref.
    base::Value::List url_pref_list;
    for (GURL url : pref.urls)
      url_pref_list.Append(url.spec());
    prefs->SetList(prefs::kURLsToRestoreOnStartup, std::move(url_pref_list));
  }
}

// static
SessionStartupPref SessionStartupPref::GetStartupPref(const Profile* profile) {
  DCHECK(profile);

  // Guest sessions should not store any state, therefore they should never
  // trigger a restore during startup.
  return profile->IsGuestSession()
             ? SessionStartupPref(SessionStartupPref::DEFAULT)
             : GetStartupPref(profile->GetPrefs());
}

// static
SessionStartupPref SessionStartupPref::GetStartupPref(
    const PrefService* prefs) {
  DCHECK(prefs);

  SessionStartupPref pref(
      PrefValueToType(prefs->GetInteger(prefs::kRestoreOnStartup)));

  // Always load the urls, even if the pref type isn't URLS. This way the
  // preferences panels can show the user their last choice.
  const base::Value::List& url_list =
      prefs->GetList(prefs::kURLsToRestoreOnStartup);
  URLListToPref(url_list, &pref);

  return pref;
}

// static
bool SessionStartupPref::TypeIsManaged(const PrefService* prefs) {
  DCHECK(prefs);
  const PrefService::Preference* pref_restore =
      prefs->FindPreference(prefs::kRestoreOnStartup);
  DCHECK(pref_restore);
  return pref_restore->IsManaged();
}

// static
bool SessionStartupPref::URLsAreManaged(const PrefService* prefs) {
  DCHECK(prefs);
  const PrefService::Preference* pref_urls =
      prefs->FindPreference(prefs::kURLsToRestoreOnStartup);
  DCHECK(pref_urls);
  return pref_urls->IsManaged();
}

// static
bool SessionStartupPref::TypeHasRecommendedValue(const PrefService* prefs) {
  DCHECK(prefs);
  const PrefService::Preference* pref_restore =
      prefs->FindPreference(prefs::kRestoreOnStartup);
  DCHECK(pref_restore);
  return pref_restore->GetRecommendedValue() != nullptr;
}

// static
bool SessionStartupPref::TypeIsDefault(const PrefService* prefs) {
  DCHECK(prefs);
  const PrefService::Preference* pref_restore =
      prefs->FindPreference(prefs::kRestoreOnStartup);
  DCHECK(pref_restore);
  return pref_restore->IsDefaultValue();
}

// static
SessionStartupPref::Type SessionStartupPref::PrefValueToType(int pref_value) {
  switch (pref_value) {
    case kPrefValueLast:
      return SessionStartupPref::LAST;
    case kPrefValueURLs:
      return SessionStartupPref::URLS;
    case kPrefValueLastAndURLs:
      return SessionStartupPref::LAST_AND_URLS;
    default:
      return SessionStartupPref::DEFAULT;
  }
}

SessionStartupPref::SessionStartupPref(Type type) : type(type) {}

SessionStartupPref::SessionStartupPref(const SessionStartupPref& other) =
    default;

SessionStartupPref::~SessionStartupPref() = default;

bool SessionStartupPref::ShouldRestoreLastSession() const {
  return type == LAST || type == LAST_AND_URLS;
}

bool SessionStartupPref::ShouldOpenUrls() const {
  return type == URLS || type == LAST_AND_URLS;
}

#if !BUILDFLAG(IS_ANDROID)
StartupTabs SessionStartupPref::ToStartupTabs() const {
  StartupTabs startup_tabs;
  for (const GURL& url : urls) {
    startup_tabs.emplace_back(
        url, type == LAST_AND_URLS
                 ? StartupTab::Type::kFromLastAndUrlsStartupPref
                 : StartupTab::Type::kNormal);
  }
  return startup_tabs;
}
#endif
