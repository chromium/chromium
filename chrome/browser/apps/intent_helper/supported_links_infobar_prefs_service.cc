// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/supported_links_infobar_prefs_service.h"

#include "base/strings/string_util.h"
#include "chrome/browser/apps/intent_helper/supported_links_infobar_prefs_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {
// The pref dict is:
// {
//  ...
//  "supported_links_infobar" : {
//    "apps": {
//      <app_id_1> : {
//        "dismissed" : <bool>,
//        "ignored_count" : <int>
//      },
//      <app_id_2> : {
//        "dismissed" : <bool>
//      },
//      ...
//    },
//  },
//  ...
// }
const char kSupportedLinksAppPrefsKey[] = "supported_links_infobar.apps";

const char kInfoBarDismissedKey[] = "dismissed";
const char kInfoBarIgnoredCountKey[] = "ignored_count";
// The maximum number of times the InfoBar can be ignored before it is no longer
// shown for an app.
const char kMaxIgnoreCount = 3;

}  // namespace

namespace apps {

// static
SupportedLinksInfoBarPrefsService* SupportedLinksInfoBarPrefsService::Get(
    Profile* profile) {
  return SupportedLinksInfoBarPrefsServiceFactory::GetForProfile(profile);
}

// static
void SupportedLinksInfoBarPrefsService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSupportedLinksAppPrefsKey);
}

SupportedLinksInfoBarPrefsService::SupportedLinksInfoBarPrefsService(
    Profile* profile)
    : profile_(profile) {}

SupportedLinksInfoBarPrefsService::~SupportedLinksInfoBarPrefsService() =
    default;

bool SupportedLinksInfoBarPrefsService::ShouldHideInfoBarForApp(
    const std::string& app_id) {
  const base::Value* base_pref =
      profile_->GetPrefs()->GetDictionary(kSupportedLinksAppPrefsKey);
  const base::Value::Dict* app_value = base_pref->GetDict().FindDict(app_id);

  if (app_value == nullptr) {
    return false;
  }

  if (app_value->FindBool(kInfoBarDismissedKey).value_or(false)) {
    // InfoBar has previously been dismissed.
    return true;
  }

  if (app_value->FindInt(kInfoBarIgnoredCountKey).value_or(0) >=
      kMaxIgnoreCount) {
    // InfoBar has previously been ignored multiple times.
    return true;
  }

  return false;
}

void SupportedLinksInfoBarPrefsService::MarkInfoBarDismissed(
    const std::string& app_id) {
  DictionaryPrefUpdate infobar_prefs(profile_->GetPrefs(),
                                     kSupportedLinksAppPrefsKey);

  infobar_prefs->GetDict().SetByDottedPath(
      base::JoinString({app_id, kInfoBarDismissedKey}, "."), true);
}

void SupportedLinksInfoBarPrefsService::MarkInfoBarIgnored(
    const std::string& app_id) {
  DictionaryPrefUpdate infobar_prefs(profile_->GetPrefs(),
                                     kSupportedLinksAppPrefsKey);

  auto path = base::JoinString({app_id, kInfoBarIgnoredCountKey}, ".");
  absl::optional<int> ignore_count =
      infobar_prefs->GetDict().FindIntByDottedPath(path);
  infobar_prefs->GetDict().SetByDottedPath(path, ignore_count.value_or(0) + 1);
}

}  // namespace apps
