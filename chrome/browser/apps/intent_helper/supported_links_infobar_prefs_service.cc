// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/supported_links_infobar_prefs_service.h"

#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/supported_links_infobar_prefs_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/types_util.h"

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

bool AppIsInstalled(Profile* profile, const std::string& app_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  bool installed = false;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&installed](const apps::AppUpdate& update) {
        installed = apps_util::IsInstalled(update.Readiness());
      });
  return installed;
}

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
    : profile_(profile) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);
  apps_observation_.Observe(&proxy->AppRegistryCache());
}

SupportedLinksInfoBarPrefsService::~SupportedLinksInfoBarPrefsService() =
    default;

bool SupportedLinksInfoBarPrefsService::ShouldHideInfoBarForApp(
    const std::string& app_id) {
  const base::Value::Dict& base_pref =
      profile_->GetPrefs()->GetDict(kSupportedLinksAppPrefsKey);
  const base::Value::Dict* app_value = base_pref.FindDict(app_id);

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
  if (!AppIsInstalled(profile_, app_id))
    return;

  ScopedDictPrefUpdate infobar_prefs(profile_->GetPrefs(),
                                     kSupportedLinksAppPrefsKey);

  infobar_prefs->SetByDottedPath(
      base::JoinString({app_id, kInfoBarDismissedKey}, "."), true);
}

void SupportedLinksInfoBarPrefsService::MarkInfoBarIgnored(
    const std::string& app_id) {
  if (!AppIsInstalled(profile_, app_id))
    return;

  ScopedDictPrefUpdate infobar_prefs(profile_->GetPrefs(),
                                     kSupportedLinksAppPrefsKey);

  auto path = base::JoinString({app_id, kInfoBarIgnoredCountKey}, ".");
  absl::optional<int> ignore_count = infobar_prefs->FindIntByDottedPath(path);
  infobar_prefs->SetByDottedPath(path, ignore_count.value_or(0) + 1);
}

void SupportedLinksInfoBarPrefsService::OnAppUpdate(
    const apps::AppUpdate& update) {
  if (update.ReadinessChanged() &&
      !apps_util::IsInstalled(update.Readiness())) {
    ScopedDictPrefUpdate infobar_prefs(profile_->GetPrefs(),
                                       kSupportedLinksAppPrefsKey);
    infobar_prefs->Remove(update.AppId());
  }
}

void SupportedLinksInfoBarPrefsService::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  apps_observation_.Reset();
}

}  // namespace apps
