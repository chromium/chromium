// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_DEFAULT_APP_LIST_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_DEFAULT_APP_LIST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Contains map of default pre-installed apps and packages.
class ArcDefaultAppList : public arc::ArcSessionManagerObserver {
 public:
  struct AppInfo {
    AppInfo(const std::string& name,
                   const std::string& package_name,
                   const std::string& activity,
                   bool oem,
                   const base::FilePath app_path);
    ~AppInfo();

    std::string name;
    std::string package_name;
    std::string activity;
    bool oem;
    base::FilePath app_path;  // App folder that contains pre-installed icons.
  };

  enum class FilterLevel {
    // Filter nothing.
    NOTHING,
    // Filter out only optional apps, excluding Play Store for example. Used in
    // case when Play Store is managed and enabled.
    OPTIONAL_APPS,
    // Filter out everything. Used in case when Play Store is managed and
    // disabled.
    ALL
  };

  // Defines App id to default AppInfo mapping.
  using AppInfoMap = std::map<std::string, std::unique_ptr<AppInfo>>;

  ArcDefaultAppList(Profile* profile, base::OnceClosure ready_callback);
  ArcDefaultAppList(const ArcDefaultAppList&) = delete;
  ArcDefaultAppList& operator=(const ArcDefaultAppList&) = delete;
  ~ArcDefaultAppList() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static void UseTestAppsDirectory();
  static std::string GetBoardNameForTesting(
      const base::FilePath& build_prop_path);

  // Returns default app info if it is found in defaults and its package is not
  // marked as uninstalled.
  const AppInfo* GetApp(const std::string& app_id) const;
  // Returns true if app is found in defaults and its package is not marked as
  // uninstalled.
  bool HasApp(const std::string& app_id) const;
  // Returns true if package exists in default packages list. Note it may be
  // marked as uninstalled.
  bool HasPackage(const std::string& package_name) const;
  // Returns true if a uninstalled package exists in default packages list.
  bool HasHiddenPackage(const std::string& package_name) const;
  // Marks default app |app_id| as hidden in case |hidden| is true.
  void SetAppHidden(const std::string& app_id, bool hidden);

  // Returns map of apps that are visible and not filtered.
  std::map<std::string, const AppInfo*> GetActiveApps() const;
  // Returns set of packages that covers active apps.
  std::unordered_set<std::string> GetActivePackages() const;

  void set_filter_level(FilterLevel filter_level) {
    filter_level_ = filter_level;
  }

 private:
  // arc::ArcSessionManagerObserver:
  void OnPropertyFilesExpanded(bool result) override;

  // Loads default apps from two sources:
  //
  // /usr/share/google-chrome/extensions/arc - contains default apps for all
  //     boards that share the same image.
  // /usr/share/google-chrome/extensions/arc/<BOARD_NAME> - contains default
  //     apps for particular current board.
  //
  void LoadDefaultApps(std::string board_name);

  // Called when default apps are read from the provided source.
  void OnAppsRead(std::unique_ptr<AppInfoMap> apps);
  // Called when default apps from all sources are read.
  void OnAppsReady();

  // Unowned pointer.
  raw_ptr<Profile> profile_;

  base::OnceClosure ready_callback_;

  FilterLevel filter_level_ = FilterLevel::ALL;

  // Keeps visible apps.
  AppInfoMap visible_apps_;
  // Keeps hidden apps.
  AppInfoMap hidden_apps_;
  // To wait until all sources with apps are loaded.
  base::RepeatingClosure barrier_closure_;

  base::WeakPtrFactory<ArcDefaultAppList> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_DEFAULT_APP_LIST_H_
