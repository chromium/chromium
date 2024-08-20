// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_TEST_SYSTEM_WEB_APP_INSTALLATION_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_TEST_SYSTEM_WEB_APP_INSTALLATION_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_web_ui_controller_factory.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace web_app {
class FakeWebAppProviderCreator;
}  // namespace web_app

namespace ash {

class UnittestingSystemAppDelegate : public SystemWebAppDelegate {
 public:
  UnittestingSystemAppDelegate(SystemWebAppType type,
                               const std::string& name,
                               const GURL& url,
                               web_app::WebAppInstallInfoFactory info_factory);
  UnittestingSystemAppDelegate(const UnittestingSystemAppDelegate&) = delete;
  UnittestingSystemAppDelegate& operator=(const UnittestingSystemAppDelegate&) =
      delete;
  ~UnittestingSystemAppDelegate() override;

  using LaunchAndNavigateSystemWebAppCallback =
      base::RepeatingCallback<Browser*(Profile*,
                                       web_app::WebAppProvider*,
                                       const GURL&,
                                       const apps::AppLaunchParams&)>;

  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;

  std::vector<std::string> GetAppIdsToUninstallAndReplace() const override;
  gfx::Size GetMinimumWindowSize() const override;
  Browser* GetWindowForLaunch(Profile* profile, const GURL& url) const override;
  bool ShouldShowNewWindowMenuOption() const override;
  base::FilePath GetLaunchDirectory(
      const apps::AppLaunchParams& params) const override;
  std::vector<int> GetAdditionalSearchTerms() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearchAndShelf() const override;
  bool ShouldHandleFileOpenIntents() const override;
  bool ShouldCaptureNavigations() const override;
  bool ShouldAllowResize() const override;
  bool ShouldAllowMaximize() const override;
  bool ShouldAllowFullscreen() const override;
  bool ShouldHaveTabStrip() const override;
  bool ShouldHideNewTabButton() const override;
  bool ShouldHaveReloadButtonInMinimalUi() const override;
  bool ShouldAllowScriptsToCloseWindows() const override;
  std::optional<SystemWebAppBackgroundTaskInfo> GetTimerInfo() const override;
  gfx::Rect GetDefaultBounds(Browser* browser) const override;
  Browser* LaunchAndNavigateSystemWebApp(
      Profile* profile,
      web_app::WebAppProvider* provider,
      const GURL& url,
      const apps::AppLaunchParams& params) const override;
  bool IsAppEnabled() const override;
  bool IsUrlInSystemAppScope(const GURL& url) const override;
  bool UseSystemThemeColor() const override;
#if BUILDFLAG(IS_CHROMEOS)
  bool ShouldAnimateThemeChanges() const override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  void SetAppIdsToUninstallAndReplace(const std::vector<webapps::AppId>&);
  void SetMinimumWindowSize(const gfx::Size&);
  void SetShouldReuseExistingWindow(bool);
  void SetShouldShowNewWindowMenuOption(bool);
  void SetShouldIncludeLaunchDirectory(bool);
  void SetEnabledOriginTrials(const OriginTrialsMap&);
  void SetAdditionalSearchTerms(const std::vector<int>&);
  void SetShouldShowInLauncher(bool);
  void SetShouldShowInSearchAndShelf(bool);
  void SetShouldHandleFileOpenIntents(bool);
  void SetShouldCaptureNavigations(bool);
  void SetShouldAllowResize(bool);
  void SetShouldAllowMaximize(bool);
  void SetShouldHaveTabStrip(bool);
  void SetShouldHideNewTabButton(bool);
  void SetShouldHaveReloadButtonInMinimalUi(bool);
  void SetShouldAllowScriptsToCloseWindows(bool);
  void SetTimerInfo(const SystemWebAppBackgroundTaskInfo&);
  void SetDefaultBounds(base::RepeatingCallback<gfx::Rect(Browser*)>);
  void SetLaunchAndNavigateSystemWebApp(LaunchAndNavigateSystemWebAppCallback);
  void SetIsAppEnabled(bool);
  void SetUrlInSystemAppScope(const GURL& url);
  void SetUseSystemThemeColor(bool);
#if BUILDFLAG(IS_CHROMEOS)
  void SetShouldAnimateThemeChanges(bool);
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  web_app::WebAppInstallInfoFactory info_factory_;

  std::vector<webapps::AppId> uninstall_and_replace_;
  gfx::Size minimum_window_size_;
  bool single_window_ = true;
  bool show_new_window_menu_option_ = false;
  bool include_launch_directory_ = false;
  std::vector<int> additional_search_terms_;
  bool show_in_launcher_ = true;
  bool show_in_search_ = true;
  bool handles_file_open_intents_ = false;
  bool capture_navigations_ = false;
  bool is_resizeable_ = true;
  bool is_maximizable_ = true;
  bool is_fullscreenable_ = true;
  bool has_tab_strip_ = false;
  bool hide_new_tab_button_ = false;
  bool should_have_reload_button_in_minimal_ui_ = true;
  bool allow_scripts_to_close_windows_ = false;
  bool is_app_enabled = true;
  GURL url_in_system_app_scope_;
  bool use_system_theme_color_ = true;
#if BUILDFLAG(IS_CHROMEOS)
  bool should_animate_theme_changes_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::RepeatingCallback<gfx::Rect(Browser*)> get_default_bounds_ =
      base::NullCallback();

  LaunchAndNavigateSystemWebAppCallback launch_and_navigate_system_web_apps_ =
      base::NullCallback();

  std::optional<SystemWebAppBackgroundTaskInfo> timer_info_;
};

// Class to setup the installation of a test System Web App.
//
// Use SetUp*() methods to create a instance of this class in test suite's
// constructor, before the profile is fully created. In tests, call
// WaitForAppInstall() to finish the installation.
class TestSystemWebAppInstallation {
 public:
  enum IncludeLaunchDirectory { kYes, kNo };

  // Used for tests that don't want to install any System Web Apps.
  static std::unique_ptr<TestSystemWebAppInstallation> SetUpWithoutApps();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpTabbedMultiWindowApp();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpStandaloneSingleWindowApp();

  // This method automatically grants File System Access read and write
  // permissions to the App.
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppThatReceivesLaunchFiles(
      IncludeLaunchDirectory include_launch_directory);

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithEnabledOriginTrials(const OriginTrialsMap& origin_to_trials);

  static std::unique_ptr<TestSystemWebAppInstallation> SetUpAppLaunchWithUrl();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppNotShownInLauncher();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppNotShownInSearch();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppThatHandlesFileOpenIntents();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithAdditionalSearchTerms();

  // This method additionally sets up a helper SystemWebAppType::SETTING
  // system app for testing capturing links from a different SWA.
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppThatCapturesNavigation();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpChromeUntrustedApp();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpNonResizeableAndNonMaximizableApp();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithBackgroundTask();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetupAppWithAllowScriptsToCloseWindows(bool value);

  static std::unique_ptr<TestSystemWebAppInstallation> SetUpAppWithTabStrip(
      bool has_tab_strip,
      bool hide_new_tab_button);

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithDefaultBounds(const gfx::Rect& default_bounds);

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithNewWindowMenuItem();

  static std::unique_ptr<TestSystemWebAppInstallation> SetUpAppWithShortcuts();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppThatAbortsLaunch();

  // This creates 4 system web app types for testing context menu with
  // different windowing options:
  //
  // - SETTINGS: Single Window, No TabStrip
  // - FILE_MANAGER: Multi Window, No TabStrip
  // - MEDIA: Single Window, TabStrip
  // - HELP: Multi Window, TabStrip
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppsForContestMenuTest();

  static std::unique_ptr<TestSystemWebAppInstallation> SetUpAppWithColors(
      std::optional<SkColor> theme_color,
      std::optional<SkColor> dark_mode_theme_color,
      std::optional<SkColor> background_color,
      std::optional<SkColor> dark_mode_background_color);

  static std::unique_ptr<TestSystemWebAppInstallation> SetUpAppWithValidIcons();

  ~TestSystemWebAppInstallation();

  void WaitForAppInstall();

  webapps::AppId GetAppId();
  const GURL& GetAppUrl();
  SystemWebAppDelegate* GetDelegate();
  SystemWebAppType GetType();

  void set_update_policy(SystemWebAppManager::UpdatePolicy update_policy) {
    update_policy_ = update_policy;
  }

 private:
  explicit TestSystemWebAppInstallation(
      std::unique_ptr<UnittestingSystemAppDelegate> system_app_delegate);
  TestSystemWebAppInstallation();

  std::unique_ptr<KeyedService> CreateWebAppProvider(Profile* profile);
  std::unique_ptr<KeyedService> CreateSystemWebAppManager(
      UnittestingSystemAppDelegate* system_app_delegate,
      Profile* profile);

  std::unique_ptr<KeyedService> CreateSystemWebAppManagerWithNoSystemWebApps(
      Profile* profile);

  // Must be called in SetUp*App() methods, before WebAppProvider is created.
  void RegisterAutoGrantedPermissions(ContentSettingsType permission);

  raw_ptr<Profile, DanglingUntriaged> profile_;
  SystemWebAppManager::UpdatePolicy update_policy_ =
      SystemWebAppManager::UpdatePolicy::kAlwaysUpdate;

  std::unique_ptr<web_app::FakeWebAppProviderCreator>
      fake_web_app_provider_creator_;
  std::unique_ptr<TestSystemWebAppManagerCreator>
      test_system_web_app_manager_creator_;

  // nullopt if SetUpWithoutApps() was used.
  const std::optional<SystemWebAppType> type_;
  std::vector<std::unique_ptr<TestSystemWebAppWebUIControllerFactory>>
      web_ui_controller_factories_;
  std::set<ContentSettingsType> auto_granted_permissions_;
  base::flat_map<SystemWebAppType, std::unique_ptr<SystemWebAppDelegate>>
      system_app_delegates_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_TEST_SYSTEM_WEB_APP_INSTALLATION_H_
