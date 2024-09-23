// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_url_data_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/url_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

namespace {

enum class WebUIType {
  // kChrome WebUIs registration works by creating a WebUIControllerFactory
  // which then register a URLDataSource to serve resources.
  kChrome,
  // kChromeUntrusted WebUIs don't have WebUIControllers and their
  // URLDataSources need to be registered directly.
  kChromeUntrusted,
};

WebUIType GetWebUIType(const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme))
    return WebUIType::kChrome;
  if (url.SchemeIs(content::kChromeUIUntrustedScheme))
    return WebUIType::kChromeUntrusted;
  NOTREACHED_IN_MIGRATION();
  return WebUIType::kChrome;
}

// Assumes url is like "chrome://web-app/index.html". Returns "web-app";
// This function is needed because at the time TestSystemWebInstallation is
// initialized, chrome scheme is not yet registered with GURL, so it will be
// parsed as PathURL, resulting in an empty host.
std::string GetDataSourceNameFromSystemAppInstallUrl(const GURL& url) {
  DCHECK(url.SchemeIs(content::kChromeUIScheme));

  const std::string& spec = url.spec();
  size_t p = strlen(content::kChromeUIScheme);

  DCHECK_EQ("://", spec.substr(p, 3));
  p += 3;

  size_t pos_after_host = spec.find('/', p);
  DCHECK(pos_after_host != std::string::npos);

  return spec.substr(p, pos_after_host - p);
}

// Returns the scheme and host from an install URL e.g. for
// chrome-untrusted://web-app/index.html this returns
// chrome-untrusted://web-app/.
std::string GetChromeUntrustedDataSourceNameFromInstallUrl(const GURL& url) {
  DCHECK(url.SchemeIs(content::kChromeUIUntrustedScheme));

  const std::string& spec = url.spec();
  size_t p = strlen(content::kChromeUIUntrustedScheme);
  DCHECK_EQ("://", spec.substr(p, 3));
  p += 3;

  size_t pos_after_host = spec.find('/', p);
  DCHECK(pos_after_host != std::string::npos);

  // The Data Source name must include "/" after the host.
  ++pos_after_host;
  return spec.substr(0, pos_after_host);
}

}  // namespace

UnittestingSystemAppDelegate::UnittestingSystemAppDelegate(
    SystemWebAppType type,
    const std::string& name,
    const GURL& url,
    web_app::WebAppInstallInfoFactory info_factory)
    : SystemWebAppDelegate(type, name, url, nullptr),
      info_factory_(std::move(info_factory)) {}

UnittestingSystemAppDelegate::~UnittestingSystemAppDelegate() = default;

std::unique_ptr<web_app::WebAppInstallInfo>
UnittestingSystemAppDelegate::GetWebAppInfo() const {
  return info_factory_.Run();
}

std::vector<std::string>
UnittestingSystemAppDelegate::GetAppIdsToUninstallAndReplace() const {
  return uninstall_and_replace_;
}

gfx::Size UnittestingSystemAppDelegate::GetMinimumWindowSize() const {
  return minimum_window_size_;
}
Browser* UnittestingSystemAppDelegate::GetWindowForLaunch(
    Profile* profile,
    const GURL& url) const {
  return single_window_ ? SystemWebAppDelegate::GetWindowForLaunch(profile, url)
                        : nullptr;
}
bool UnittestingSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return show_new_window_menu_option_;
}
base::FilePath UnittestingSystemAppDelegate::GetLaunchDirectory(
    const apps::AppLaunchParams& params) const {
  // When set to include a launch directory, use the directory of the first
  // file.
  return include_launch_directory_ && !params.launch_files.empty()
             ? params.launch_files[0].DirName()
             : base::FilePath();
}

std::vector<int> UnittestingSystemAppDelegate::GetAdditionalSearchTerms()
    const {
  return additional_search_terms_;
}
bool UnittestingSystemAppDelegate::ShouldShowInLauncher() const {
  return show_in_launcher_;
}
bool UnittestingSystemAppDelegate::ShouldShowInSearchAndShelf() const {
  return show_in_search_;
}
bool UnittestingSystemAppDelegate::ShouldHandleFileOpenIntents() const {
  return handles_file_open_intents_;
}
bool UnittestingSystemAppDelegate::ShouldCaptureNavigations() const {
  return capture_navigations_;
}
bool UnittestingSystemAppDelegate::ShouldAllowResize() const {
  return is_resizeable_;
}
bool UnittestingSystemAppDelegate::ShouldAllowMaximize() const {
  return is_maximizable_;
}
bool UnittestingSystemAppDelegate::ShouldAllowFullscreen() const {
  return is_fullscreenable_;
}
bool UnittestingSystemAppDelegate::ShouldHaveTabStrip() const {
  return has_tab_strip_;
}
bool UnittestingSystemAppDelegate::ShouldHideNewTabButton() const {
  return hide_new_tab_button_;
}
bool UnittestingSystemAppDelegate::ShouldHaveReloadButtonInMinimalUi() const {
  return should_have_reload_button_in_minimal_ui_;
}
bool UnittestingSystemAppDelegate::ShouldAllowScriptsToCloseWindows() const {
  return allow_scripts_to_close_windows_;
}
std::optional<SystemWebAppBackgroundTaskInfo>
UnittestingSystemAppDelegate::GetTimerInfo() const {
  return timer_info_;
}
gfx::Rect UnittestingSystemAppDelegate::GetDefaultBounds(
    Browser* browser) const {
  if (get_default_bounds_) {
    return get_default_bounds_.Run(browser);
  }
  return gfx::Rect();
}

Browser* UnittestingSystemAppDelegate::LaunchAndNavigateSystemWebApp(
    Profile* profile,
    web_app::WebAppProvider* provider,
    const GURL& url,
    const apps::AppLaunchParams& params) const {
  if (launch_and_navigate_system_web_apps_) {
    return launch_and_navigate_system_web_apps_.Run(profile, provider, url,
                                                    params);
  }
  return SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(profile, provider,
                                                             url, params);
}

bool UnittestingSystemAppDelegate::IsAppEnabled() const {
  return is_app_enabled;
}
bool UnittestingSystemAppDelegate::IsUrlInSystemAppScope(
    const GURL& url) const {
  return url == url_in_system_app_scope_;
}
bool UnittestingSystemAppDelegate::UseSystemThemeColor() const {
  return use_system_theme_color_;
}
#if BUILDFLAG(IS_CHROMEOS)
bool UnittestingSystemAppDelegate::ShouldAnimateThemeChanges() const {
  return should_animate_theme_changes_;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void UnittestingSystemAppDelegate::SetAppIdsToUninstallAndReplace(
    const std::vector<webapps::AppId>& ids) {
  uninstall_and_replace_ = ids;
}
void UnittestingSystemAppDelegate::SetMinimumWindowSize(const gfx::Size& size) {
  minimum_window_size_ = size;
}
void UnittestingSystemAppDelegate::SetShouldReuseExistingWindow(bool value) {
  single_window_ = value;
}
void UnittestingSystemAppDelegate::SetShouldShowNewWindowMenuOption(
    bool value) {
  show_new_window_menu_option_ = value;
}
void UnittestingSystemAppDelegate::SetShouldIncludeLaunchDirectory(bool value) {
  include_launch_directory_ = value;
}
void UnittestingSystemAppDelegate::SetEnabledOriginTrials(
    const OriginTrialsMap& map) {
  origin_trials_map_ = map;
}
void UnittestingSystemAppDelegate::SetAdditionalSearchTerms(
    const std::vector<int>& terms) {
  additional_search_terms_ = terms;
}
void UnittestingSystemAppDelegate::SetShouldShowInLauncher(bool value) {
  show_in_launcher_ = value;
}
void UnittestingSystemAppDelegate::SetShouldShowInSearchAndShelf(bool value) {
  show_in_search_ = value;
}
void UnittestingSystemAppDelegate::SetShouldHandleFileOpenIntents(bool value) {
  handles_file_open_intents_ = value;
}
void UnittestingSystemAppDelegate::SetShouldCaptureNavigations(bool value) {
  capture_navigations_ = value;
}
void UnittestingSystemAppDelegate::SetShouldAllowResize(bool value) {
  is_resizeable_ = value;
}
void UnittestingSystemAppDelegate::SetShouldAllowMaximize(bool value) {
  is_maximizable_ = value;
}
void UnittestingSystemAppDelegate::SetShouldHaveTabStrip(bool value) {
  has_tab_strip_ = value;
}
void UnittestingSystemAppDelegate::SetShouldHideNewTabButton(bool value) {
  hide_new_tab_button_ = value;
}
void UnittestingSystemAppDelegate::SetShouldHaveReloadButtonInMinimalUi(
    bool value) {
  should_have_reload_button_in_minimal_ui_ = value;
}
void UnittestingSystemAppDelegate::SetShouldAllowScriptsToCloseWindows(
    bool value) {
  allow_scripts_to_close_windows_ = value;
}
void UnittestingSystemAppDelegate::SetTimerInfo(
    const SystemWebAppBackgroundTaskInfo& timer_info) {
  timer_info_ = timer_info;
}
void UnittestingSystemAppDelegate::SetDefaultBounds(
    base::RepeatingCallback<gfx::Rect(Browser*)> lambda) {
  get_default_bounds_ = std::move(lambda);
}
void UnittestingSystemAppDelegate::SetLaunchAndNavigateSystemWebApp(
    LaunchAndNavigateSystemWebAppCallback lambda) {
  launch_and_navigate_system_web_apps_ = std::move(lambda);
}
void UnittestingSystemAppDelegate::SetIsAppEnabled(bool value) {
  is_app_enabled = value;
}
void UnittestingSystemAppDelegate::SetUrlInSystemAppScope(const GURL& url) {
  url_in_system_app_scope_ = url;
}
void UnittestingSystemAppDelegate::SetUseSystemThemeColor(bool value) {
  use_system_theme_color_ = value;
}
#if BUILDFLAG(IS_CHROMEOS)
void UnittestingSystemAppDelegate::SetShouldAnimateThemeChanges(bool value) {
  should_animate_theme_changes_ = value;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TestSystemWebAppInstallation::TestSystemWebAppInstallation(
    std::unique_ptr<UnittestingSystemAppDelegate> delegate)
    : type_(delegate->GetType()) {
  if (GetWebUIType(delegate->GetInstallUrl()) == WebUIType::kChrome) {
    auto factory = std::make_unique<TestSystemWebAppWebUIControllerFactory>(
        GetDataSourceNameFromSystemAppInstallUrl(delegate->GetInstallUrl()));
    web_ui_controller_factories_.push_back(std::move(factory));
  }

  UnittestingSystemAppDelegate* delegate_ptr = delegate.get();
  system_app_delegates_.emplace(type_.value(), std::move(delegate));

  fake_web_app_provider_creator_ =
      std::make_unique<web_app::FakeWebAppProviderCreator>(base::BindRepeating(
          &TestSystemWebAppInstallation::CreateWebAppProvider,
          // base::Unretained is safe here. This callback is
          // called at TestingProfile::Init, which is at test
          // startup. TestSystemWebAppInstallation is intended
          // to have the same lifecycle as the test, it won't be
          // destroyed before the test finishes.
          base::Unretained(this)));

  test_system_web_app_manager_creator_ =
      std::make_unique<TestSystemWebAppManagerCreator>(base::BindRepeating(
          &TestSystemWebAppInstallation::CreateSystemWebAppManager,
          base::Unretained(this), delegate_ptr));
}

TestSystemWebAppInstallation::TestSystemWebAppInstallation() {
  fake_web_app_provider_creator_ =
      std::make_unique<web_app::FakeWebAppProviderCreator>(base::BindRepeating(
          &TestSystemWebAppInstallation::CreateWebAppProvider,
          // base::Unretained is safe here. This callback is
          // called at TestingProfile::Init, which is at test
          // startup. TestSystemWebAppInstallation is intended
          // to have the same lifecycle as the test, it won't be
          // destroyed before the test finishes.
          base::Unretained(this)));

  test_system_web_app_manager_creator_ =
      std::make_unique<TestSystemWebAppManagerCreator>(
          base::BindRepeating(&TestSystemWebAppInstallation::
                                  CreateSystemWebAppManagerWithNoSystemWebApps,
                              base::Unretained(this)));
}

TestSystemWebAppInstallation::~TestSystemWebAppInstallation() = default;

std::unique_ptr<web_app::WebAppInstallInfo>
GenerateWebAppInstallInfoForTestApp() {
  // the pwa.html is arguably wrong, but the manifest version uses it
  // incorrectly as well, and it's a lot of work to fix it. App ids are
  // generated from this, and it's important to keep it stable across the
  // installation modes.
  auto start_url = GURL("chrome://test-system-app/pwa.html");
  auto info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->scope = GURL("chrome://test-system-app/");
  info->title = u"Test System App";
  info->theme_color = 0xFF00FF00;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
  info->install_url = GURL("chrome://test-system-app/pwa.html");
  return info;
}

std::unique_ptr<web_app::WebAppInstallInfo>
GenerateWebAppInstallInfoForTestAppUntrusted() {
  auto start_url = GURL("chrome-untrusted://test-system-app/pwa.html");
  auto info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->scope = GURL("chrome-untrusted://test-system-app/");
  info->title = u"Test System App Untrusted";
  info->theme_color = 0xFFFF0000;
  return info;
}

SkBitmap CreateIcon(int size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(SK_ColorBLUE);
  return bitmap;
}

std::unique_ptr<web_app::WebAppInstallInfo>
GenerateWebAppInstallInfoWithValidIcons() {
  auto info = GenerateWebAppInstallInfoForTestApp();
  info->manifest_icons.emplace_back(info->start_url().Resolve("test.png"),
                                    web_app::icon_size::k256);
  info->icon_bitmaps.any[web_app::icon_size::k256] =
      CreateIcon(web_app::icon_size::k256);
  return info;
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpWithoutApps() {
  return base::WrapUnique(new TestSystemWebAppInstallation());
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpTabbedMultiWindowApp() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::TERMINAL, "Terminal",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldReuseExistingWindow(false);
  delegate->SetShouldHaveTabStrip(true);
  // Terminal, the only tabbed SWA ATM never uses system colors (or it wouldn't
  // get per tab coloring).
  delegate->SetUseSystemThemeColor(false);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::SETTINGS, "OSSettings",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetUrlInSystemAppScope(GURL("http://example.com/in-scope"));

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppThatReceivesLaunchFiles(
    IncludeLaunchDirectory include_launch_directory) {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Media",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));

  if (include_launch_directory == IncludeLaunchDirectory::kYes) {
    delegate->SetShouldIncludeLaunchDirectory(true);
  }

  auto* installation = new TestSystemWebAppInstallation(std::move(delegate));
  installation->RegisterAutoGrantedPermissions(
      ContentSettingsType::FILE_SYSTEM_READ_GUARD);
  installation->RegisterAutoGrantedPermissions(
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);

  return base::WrapUnique(installation);
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithEnabledOriginTrials(
    const OriginTrialsMap& origin_to_trials) {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Media",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));

  delegate->SetEnabledOriginTrials(origin_to_trials);
  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppLaunchWithUrl() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));

  delegate->SetShouldShowInLauncher(false);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppNotShownInLauncher() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));

  delegate->SetShouldShowInLauncher(false);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppNotShownInSearch() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldShowInSearchAndShelf(false);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppThatHandlesFileOpenIntents() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldHandleFileOpenIntents(true);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithAdditionalSearchTerms() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetAdditionalSearchTerms({IDS_SETTINGS_SECURITY});

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppThatCapturesNavigation() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::HELP, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldCaptureNavigations(true);

  auto* installation = new TestSystemWebAppInstallation(std::move(delegate));

  // Add a helper system app to test capturing links from it.
  const GURL kInitiatingAppUrl = GURL("chrome://initiating-app/pwa.html");
  installation->system_app_delegates_.insert_or_assign(
      SystemWebAppType::SETTINGS,
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::SETTINGS, "Initiating App", kInitiatingAppUrl,
          base::BindLambdaForTesting([]() {
            // the pwa.html is arguably wrong, but the manifest
            // version uses it incorrectly as well, and it's a lot of
            // work to fix it. App ids are generated from this, and
            // it's important to keep it stable across the
            // installation modes.
            auto info =
                web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
                    GURL("chrome://initiating-app/pwa.html"));
            info->scope = GURL("chrome://initiating-app/");
            info->title = u"Test System App";
            info->theme_color = 0xFF00FF00;
            info->display_mode = blink::mojom::DisplayMode::kStandalone;
            info->user_display_mode =
                web_app::mojom::UserDisplayMode::kStandalone;
            info->install_url = GURL("chrome://initiating-app/pwa.html");
            return info;
          })));
  auto factory = std::make_unique<TestSystemWebAppWebUIControllerFactory>(
      kInitiatingAppUrl.host());
  installation->web_ui_controller_factories_.push_back(std::move(factory));

  return base::WrapUnique(installation);
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpChromeUntrustedApp() {
  return base::WrapUnique(new TestSystemWebAppInstallation(
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::SETTINGS, "Test",
          GURL("chrome-untrusted://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestAppUntrusted))));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpNonResizeableAndNonMaximizableApp() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldAllowResize(false);
  delegate->SetShouldAllowMaximize(false);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithBackgroundTask() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));

  SystemWebAppBackgroundTaskInfo background_task(
      base::Days(1), GURL("chrome://test-system-app/page2.html"), true);
  delegate->SetTimerInfo(background_task);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetupAppWithAllowScriptsToCloseWindows(
    bool value) {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  /* The default value of allow_scripts_to_close_windows is false. */
  if (value) {
    delegate->SetShouldAllowScriptsToCloseWindows(value);
  }
  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithTabStrip(bool has_tab_strip,
                                                   bool hide_new_tab_button) {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldHaveTabStrip(has_tab_strip);
  delegate->SetShouldHideNewTabButton(hide_new_tab_button);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithDefaultBounds(
    const gfx::Rect& default_bounds) {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetDefaultBounds(
      base::BindLambdaForTesting([&](Browser*) { return default_bounds; }));

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithNewWindowMenuItem() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::FILE_MANAGER, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldShowNewWindowMenuOption(true);
  delegate->SetShouldReuseExistingWindow(false);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithShortcuts() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::SHORTCUT_CUSTOMIZATION, "Shortcuts",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindLambdaForTesting([]() {
            std::unique_ptr<web_app::WebAppInstallInfo> info =
                GenerateWebAppInstallInfoForTestApp();
            info->title = u"Shortcuts";
            {
              web_app::WebAppShortcutsMenuItemInfo menu_item;
              menu_item.name = u"One";
              menu_item.url = GURL("chrome://test-system-app/pwa.html#one");
              info->shortcuts_menu_item_infos.push_back(std::move(menu_item));

              web_app::IconBitmaps bitmaps;
              bitmaps.any[web_app::icon_size::k256] =
                  CreateIcon(web_app::icon_size::k256);
              info->shortcuts_menu_icon_bitmaps.push_back(bitmaps);
            }
            {
              web_app::WebAppShortcutsMenuItemInfo menu_item;
              menu_item.name = u"Two";
              menu_item.url = GURL("chrome://test-system-app/pwa.html#two");
              info->shortcuts_menu_item_infos.push_back(std::move(menu_item));

              web_app::IconBitmaps bitmaps;
              bitmaps.any[web_app::icon_size::k256] =
                  CreateIcon(web_app::icon_size::k256);
              info->shortcuts_menu_icon_bitmaps.push_back(bitmaps);
            }
            return info;
          }));

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppThatAbortsLaunch() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::OS_FEEDBACK, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetLaunchAndNavigateSystemWebApp(base::BindRepeating(
      [](Profile*, web_app::WebAppProvider*, const GURL&,
         const apps::AppLaunchParams&) -> Browser* { return nullptr; }));

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

namespace {
enum SystemWebAppWindowConfig {
  SINGLE_WINDOW,
  SINGLE_WINDOW_TAB_STRIP,
  MULTI_WINDOW,
  MULTI_WINDOW_TAB_STRIP,
};

std::unique_ptr<UnittestingSystemAppDelegate>
CreateSystemAppDelegateWithWindowConfig(
    const SystemWebAppType type,
    const GURL& app_url,
    SystemWebAppWindowConfig window_config) {
  auto* delegate = new UnittestingSystemAppDelegate(
      type, "Test App", app_url, base::BindLambdaForTesting([=]() {
        auto info =
            web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
        info->scope = app_url.DeprecatedGetOriginAsURL();
        info->title = u"Test System App";
        info->theme_color = 0xFF00FF00;
        info->display_mode = blink::mojom::DisplayMode::kStandalone;
        info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
        return info;
      }));

  switch (window_config) {
    case SystemWebAppWindowConfig::SINGLE_WINDOW:
      delegate->SetShouldReuseExistingWindow(true);
      delegate->SetShouldHaveTabStrip(false);
      break;
    case SystemWebAppWindowConfig::SINGLE_WINDOW_TAB_STRIP:
      delegate->SetShouldReuseExistingWindow(true);
      delegate->SetShouldHaveTabStrip(true);
      break;
    case SystemWebAppWindowConfig::MULTI_WINDOW:
      delegate->SetShouldReuseExistingWindow(false);
      delegate->SetShouldHaveTabStrip(false);
      break;
    case SystemWebAppWindowConfig::MULTI_WINDOW_TAB_STRIP:
      delegate->SetShouldReuseExistingWindow(false);
      delegate->SetShouldHaveTabStrip(true);
      break;
  }

  return base::WrapUnique(delegate);
}

}  // namespace

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppsForContestMenuTest() {
  std::vector<std::unique_ptr<UnittestingSystemAppDelegate>> delegates;
  delegates.push_back(CreateSystemAppDelegateWithWindowConfig(
      SystemWebAppType::SETTINGS, GURL("chrome://single-window/pwa.html"),
      SystemWebAppWindowConfig::SINGLE_WINDOW));

  delegates.push_back(CreateSystemAppDelegateWithWindowConfig(
      SystemWebAppType::FILE_MANAGER, GURL("chrome://multi-window/pwa.html"),
      SystemWebAppWindowConfig::MULTI_WINDOW));

  delegates.push_back(CreateSystemAppDelegateWithWindowConfig(
      SystemWebAppType::MEDIA,
      GURL("chrome://single-window-tab-strip/pwa.html"),
      SystemWebAppWindowConfig::SINGLE_WINDOW_TAB_STRIP));

  delegates.push_back(CreateSystemAppDelegateWithWindowConfig(
      SystemWebAppType::HELP, GURL("chrome://multi-window-tab-strip/pwa.html"),
      SystemWebAppWindowConfig::MULTI_WINDOW_TAB_STRIP));
  auto* installation =
      new TestSystemWebAppInstallation(std::move(delegates[0]));

  for (size_t i = 1; i < delegates.size(); ++i) {
    auto& delegate = delegates[i];
    installation->web_ui_controller_factories_.push_back(
        std::make_unique<TestSystemWebAppWebUIControllerFactory>(
            GetDataSourceNameFromSystemAppInstallUrl(
                delegate->GetInstallUrl())));

    installation->system_app_delegates_.insert_or_assign(delegate->GetType(),
                                                         std::move(delegate));
  }

  return base::WrapUnique(installation);
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithColors(
    std::optional<SkColor> theme_color,
    std::optional<SkColor> dark_mode_theme_color,
    std::optional<SkColor> background_color,
    std::optional<SkColor> dark_mode_background_color) {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindLambdaForTesting([=]() {
            auto info = GenerateWebAppInstallInfoForTestApp();
            info->theme_color = theme_color;
            info->dark_mode_theme_color = dark_mode_theme_color;
            info->background_color = background_color;
            info->dark_mode_background_color = dark_mode_background_color;
            return info;
          }));
  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithValidIcons() {
  auto delegate = std::make_unique<UnittestingSystemAppDelegate>(
      SystemWebAppType::SETTINGS, "Test",
      GURL("chrome://test-system-app/pwa.html"), base::BindRepeating([]() {
        return GenerateWebAppInstallInfoWithValidIcons();
      }));

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

std::unique_ptr<KeyedService>
TestSystemWebAppInstallation::CreateWebAppProvider(Profile* profile) {
  profile_ = profile;

  auto provider = std::make_unique<web_app::FakeWebAppProvider>(profile);
  provider->SetWebAppUiManager(
      std::make_unique<web_app::WebAppUiManagerImpl>(profile));
  provider->StartWithSubsystems();

  return provider;
}

std::unique_ptr<KeyedService>
TestSystemWebAppInstallation::CreateSystemWebAppManager(
    UnittestingSystemAppDelegate* delegate,
    Profile* profile) {
  // `CreateWebAppProvider` gets called first and assigns `profile_`.
  DCHECK_EQ(profile_, profile);

  if (GetWebUIType(delegate->GetInstallUrl()) == WebUIType::kChromeUntrusted) {
    AddTestURLDataSource(GetChromeUntrustedDataSourceNameFromInstallUrl(
                             delegate->GetInstallUrl()),
                         profile);
  }

  auto system_web_app_manager = std::make_unique<SystemWebAppManager>(profile);

  system_web_app_manager->SetSystemAppsForTesting(
      std::move(system_app_delegates_));
  system_web_app_manager->SetUpdatePolicyForTesting(update_policy_);

  system_web_app_manager->ScheduleStart();

  const url::Origin app_origin = url::Origin::Create(delegate->GetInstallUrl());
  auto* allowlist = WebUIAllowlist::GetOrCreate(profile);
  for (const auto& permission : auto_granted_permissions_)
    allowlist->RegisterAutoGrantedPermission(app_origin, permission);

  return system_web_app_manager;
}

std::unique_ptr<KeyedService>
TestSystemWebAppInstallation::CreateSystemWebAppManagerWithNoSystemWebApps(
    Profile* profile) {
  // `CreateWebAppProvider` gets called first and assigns `profile_`.
  DCHECK_EQ(profile_, profile);

  auto system_web_app_manager = std::make_unique<SystemWebAppManager>(profile);

  system_web_app_manager->SetSystemAppsForTesting({});
  system_web_app_manager->SetUpdatePolicyForTesting(update_policy_);

  system_web_app_manager->ScheduleStart();

  return system_web_app_manager;
}

void TestSystemWebAppInstallation::WaitForAppInstall() {
  base::RunLoop run_loop;
  SystemWebAppManager::GetForTest(profile_)->on_apps_synchronized().Post(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Wait one execution loop for on_apps_synchronized() to be
        // called on all listeners.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, run_loop.QuitClosure());
      }));
  run_loop.Run();
}

webapps::AppId TestSystemWebAppInstallation::GetAppId() {
  return SystemWebAppManager::GetForTest(profile_)
      ->GetAppIdForSystemApp(type_.value())
      .value();
}

const GURL& TestSystemWebAppInstallation::GetAppUrl() {
  return web_app::WebAppProvider::GetForTest(profile_)
      ->registrar_unsafe()
      .GetAppStartUrl(GetAppId());
}

SystemWebAppDelegate* TestSystemWebAppInstallation::GetDelegate() {
  auto it = system_app_delegates_.find(GetType());
  return it != system_app_delegates_.end() ? it->second.get() : nullptr;
}

SystemWebAppType TestSystemWebAppInstallation::GetType() {
  return type_.value();
}

void TestSystemWebAppInstallation::RegisterAutoGrantedPermissions(
    ContentSettingsType permission) {
  auto_granted_permissions_.insert(permission);
}

}  // namespace ash
