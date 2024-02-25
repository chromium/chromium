// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_APP_SORTING_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_APP_SORTING_H_

#include <stddef.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/sync/model/string_ordinal.h"
#include "components/webapps/common/web_app_id.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

namespace web_app {
class WebApp;
class WebAppRegistrar;
}  // namespace web_app

namespace extensions {

class ChromeAppSorting : public AppSorting,
                         public ExtensionRegistryObserver,
                         public web_app::WebAppRegistrarObserver,
                         public web_app::WebAppInstallManagerObserver {
 public:
  explicit ChromeAppSorting(content::BrowserContext* browser_context);

  ChromeAppSorting(const ChromeAppSorting&) = delete;
  ChromeAppSorting& operator=(const ChromeAppSorting&) = delete;

  ~ChromeAppSorting() override;

  // AppSorting implementation:
  void InitializePageOrdinalMapFromWebApps() override;
  void FixNTPOrdinalCollisions() override;
  void EnsureValidOrdinals(
      const ExtensionId& extension_id,
      const syncer::StringOrdinal& suggested_page) override;
  bool GetDefaultOrdinals(const ExtensionId& extension_id,
                          syncer::StringOrdinal* page_ordinal,
                          syncer::StringOrdinal* app_launch_ordinal) override;
  void OnExtensionMoved(const ExtensionId& moved_extension_id,
                        const ExtensionId& predecessor_extension_id,
                        const ExtensionId& successor_extension_id) override;
  syncer::StringOrdinal GetAppLaunchOrdinal(
      const ExtensionId& extension_id) const override;
  void SetAppLaunchOrdinal(
      const ExtensionId& extension_id,
      const syncer::StringOrdinal& new_app_launch_ordinal) override;
  syncer::StringOrdinal CreateFirstAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const override;
  syncer::StringOrdinal CreateNextAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const override;
  syncer::StringOrdinal CreateFirstAppPageOrdinal() const override;
  syncer::StringOrdinal GetNaturalAppPageOrdinal() const override;
  syncer::StringOrdinal GetPageOrdinal(
      const ExtensionId& extension_id) const override;
  void SetPageOrdinal(const ExtensionId& extension_id,
                      const syncer::StringOrdinal& new_page_ordinal) override;
  void ClearOrdinals(const ExtensionId& extension_id) override;
  int PageStringOrdinalAsInteger(
      const syncer::StringOrdinal& page_ordinal) const override;
  syncer::StringOrdinal PageIntegerAsStringOrdinal(size_t page_index) override;
  void SetExtensionVisible(const ExtensionId& extension_id,
                           bool visible) override;

  // web_app::WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  // web_app::WebAppRegistrarObserver:
  void OnWebAppsWillBeUpdatedFromSync(
      const std::vector<const web_app::WebApp*>& updated_apps_state) override;
  void OnAppRegistrarDestroyed() override;

 private:
  // The StringOrdinal is the app launch ordinal and the string is the extension
  // id.
  typedef std::multimap<syncer::StringOrdinal,
                        ExtensionId,
                        syncer::StringOrdinal::LessThanFn>
      AppLaunchOrdinalMap;
  // The StringOrdinal is the page ordinal and the AppLaunchOrdinalMap is the
  // contents of that page.
  typedef std::map<
      syncer::StringOrdinal, AppLaunchOrdinalMap,
    syncer::StringOrdinal::LessThanFn> PageOrdinalMap;

  // Unit tests.
  friend class ChromeAppSortingDefaultOrdinalsBase;
  friend class ChromeAppSortingGetMinOrMaxAppLaunchOrdinalsOnPage;
  friend class ChromeAppSortingInitialize;
  friend class ChromeAppSortingInitializeWithNoApps;
  friend class ChromeAppSortingPageOrdinalMapping;
  friend class ChromeAppSortingSetExtensionVisible;

  // An enum used by GetMinOrMaxAppLaunchOrdinalsOnPage to specify which
  // value should be returned.
  enum AppLaunchOrdinalReturn {MIN_ORDINAL, MAX_ORDINAL};

  // Maps an app id to its ordinals.
  struct AppOrdinals {
    AppOrdinals();
    AppOrdinals(const AppOrdinals& other);
    ~AppOrdinals();

    syncer::StringOrdinal page_ordinal;
    syncer::StringOrdinal app_launch_ordinal;
  };
  using AppOrdinalsMap = std::map<ExtensionId, AppOrdinals>;

  // This function returns the lowest ordinal on |page_ordinal| if
  // |return_value| == AppLaunchOrdinalReturn::MIN_ORDINAL, otherwise it returns
  // the largest ordinal on |page_ordinal|. If there are no apps on the page
  // then an invalid StringOrdinal is returned. It is an error to call this
  // function with an invalid |page_ordinal|.
  syncer::StringOrdinal GetMinOrMaxAppLaunchOrdinalsOnPage(
      const syncer::StringOrdinal& page_ordinal,
      AppLaunchOrdinalReturn return_type) const;

  // Initialize the |ntp_ordinal_map_| with the page ordinals used by the
  // given extensions or by fetching web apps.
  void InitializePageOrdinalMap(
      const std::vector<ExtensionId>& extension_or_app_ids);

  // Migrates the app launcher and page index values.
  void MigrateAppIndex(const ExtensionIdList& extension_ids);

  // Called to add a new mapping value for |extension_id| with a page ordinal
  // of |page_ordinal| and a app launch ordinal of |app_launch_ordinal|. This
  // works with valid and invalid StringOrdinals.
  void AddOrdinalMapping(const ExtensionId& extension_id,
                         const syncer::StringOrdinal& page_ordinal,
                         const syncer::StringOrdinal& app_launch_ordinal);

  // Ensures |ntp_ordinal_map_| is of |minimum_size| number of entries.
  void CreateOrdinalsIfNecessary(size_t minimum_size);

  // Removes the mapping for |extension_id| with a page ordinal of
  // |page_ordinal| and a app launch ordinal of |app_launch_ordinal|. If there
  // is not matching map, nothing happens. This works with valid and invalid
  // StringOrdinals.
  void RemoveOrdinalMapping(const ExtensionId& extension_id,
                            const syncer::StringOrdinal& page_ordinal,
                            const syncer::StringOrdinal& app_launch_ordinal);

  // Syncs the extension if needed. It is an error to call this if the
  // extension is not an application.
  void SyncIfNeeded(const ExtensionId& extension_id);

  // Creates the default ordinals.
  void CreateDefaultOrdinals();

  // Returns |app_launch_ordinal| if it has no collision in the page specified
  // by |page_ordinal|. Otherwise, returns an ordinal after |app_launch_ordinal|
  // that has no conflict.
  syncer::StringOrdinal ResolveCollision(
      const syncer::StringOrdinal& page_ordinal,
      const syncer::StringOrdinal& app_launch_ordinal) const;

  // Returns the number of items in |m| visible on the new tab page.
  size_t CountItemsVisibleOnNtp(const AppLaunchOrdinalMap& m) const;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;

  const raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_ =
      nullptr;
  raw_ptr<const web_app::WebAppRegistrar, AcrossTasksDanglingUntriaged>
      web_app_registrar_ = nullptr;
  raw_ptr<web_app::WebAppSyncBridge, AcrossTasksDanglingUntriaged>
      web_app_sync_bridge_ = nullptr;
  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::WebAppRegistrarObserver>
      app_registrar_observation_{this};
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  // A map of all the StringOrdinal page ordinals mapping to the collections of
  // app launch ordinals that exist on that page. This is used for mapping
  // StringOrdinals to their Integer equivalent as well as quick lookup of the
  // any collision of on the NTP (icons with the same page and same app launch
  // ordinals). The possiblity of collisions means that a multimap must be used
  // (although the collisions must all be resolved once all the syncing is
  // done).
  PageOrdinalMap ntp_ordinal_map_;

  // Defines the default ordinals.
  AppOrdinalsMap default_ordinals_;

  // Used to construct the default ordinals once when needed instead of on
  // construction when the app order may not have been determined.
  bool default_ordinals_created_;

  // The set of extensions that don't appear in the new tab page.
  std::set<ExtensionId> ntp_hidden_extensions_;

  // Observe the ExtensionRegistry. The registry is guaranteed to outlive this
  // object, since this is owned by the ExtensionSystem, which depends on the
  // ExtensionRegistry.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};

  base::WeakPtrFactory<ChromeAppSorting> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_APP_SORTING_H_
