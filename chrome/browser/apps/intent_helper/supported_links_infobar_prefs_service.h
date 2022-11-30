// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_SUPPORTED_LINKS_INFOBAR_PREFS_SERVICE_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_SUPPORTED_LINKS_INFOBAR_PREFS_SERVICE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class PrefRegistrySimple;
class Profile;

namespace apps {

class AppUpdate;

// A KeyedService to manage the preferences for the Supported Links InfoBar.
class SupportedLinksInfoBarPrefsService
    : public KeyedService,
      public apps::AppRegistryCache::Observer {
 public:
  static SupportedLinksInfoBarPrefsService* Get(Profile* profile);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit SupportedLinksInfoBarPrefsService(Profile* profile);
  ~SupportedLinksInfoBarPrefsService() override;

  SupportedLinksInfoBarPrefsService(const SupportedLinksInfoBarPrefsService&) =
      delete;
  SupportedLinksInfoBarPrefsService* operator=(
      const SupportedLinksInfoBarPrefsService&) = delete;

  // Returns true if the InfoBar should be hidden for a particular |app_id| --
  // that is, it has been ignored or cancelled for that app in the past.
  bool ShouldHideInfoBarForApp(const std::string& app_id);

  // Records that the InfoBar for a particular |app_id| was explicitly dismissed
  // by the user (by clicking 'No Thanks'). The InfoBar will not show again for
  // that app.
  void MarkInfoBarDismissed(const std::string& app_id);

  // Records that the InfoBar for a particular |app_id| was ignored by the user.
  // The InfoBar will stop showing for an app if it is repeatedly ignored.
  void MarkInfoBarIgnored(const std::string& app_id);

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  raw_ptr<Profile> profile_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      apps_observation_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_SUPPORTED_LINKS_INFOBAR_PREFS_SERVICE_H_
