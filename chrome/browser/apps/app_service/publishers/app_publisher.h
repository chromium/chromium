// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_APP_PUBLISHER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_APP_PUBLISHER_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "components/services/app_service/public/cpp/app.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "ui/base/resource/resource_scale_factor.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_icon/compressed_icon_getter.h"
#endif

namespace apps {

struct AppLaunchParams;
class PackageId;

#if BUILDFLAG(IS_CHROMEOS_ASH)
struct PromiseApp;
using PromiseAppPtr = std::unique_ptr<PromiseApp>;
#endif

// AppPublisher parent class (in the App Service sense) for all app publishers.
// See components/services/app_service/README.md.
class AppPublisher
#if BUILDFLAG(IS_CHROMEOS_ASH)
    : public CompressedIconGetter
#endif
{
 public:
  explicit AppPublisher(AppServiceProxy* proxy);
  AppPublisher(const AppPublisher&) = delete;
  AppPublisher& operator=(const AppPublisher&) = delete;
  virtual ~AppPublisher();

  // Returns an app object from the provided parameters
  static AppPtr MakeApp(AppType app_type,
                        const std::string& app_id,
                        Readiness readiness,
                        const std::string& name,
                        InstallReason install_reason,
                        InstallSource install_source);

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Registers this AppPublisher to AppServiceProxy, allowing it to receive App
  // Service API calls. This function must be called after the object's
  // creation, and can't be called in the constructor function to avoid
  // receiving API calls before being fully constructed and ready. This should
  // be called immediately before the first call to AppPublisher::Publish that
  // sends the initial list of apps to the App Service.
  void RegisterPublisher(AppType app_type);
#endif

  // Requests an icon for an app identified by `app_id`. The icon is identified
  // by `icon_key` and parameterised by `icon_type` and `size_hint_in_dp`. If
  // `allow_placeholder_icon` is true, a default placeholder icon can be
  // returned if no other icon is available. Calls `callback` with the result.
  //
  // Publishers implementing this method should:
  //  - provide an icon as closely sized to `size_hint_in_dp` as possible
  //  - load from the specific resource ID if `icon_key.resource_id` is set
  //  - may optionally use `icon_key`'s timeline property as a "version number"
  //    for an icon. Alternatively, this may be ignored if there will only ever
  //    be one version of an icon at any time.
  //  - optionally return a placeholder default icon if `allow_placeholder_icon`
  //    is true and when no icon is available for the app (or an icon for the
  //    app cannot be efficiently provided). Otherwise, a null icon should
  //    be returned.
  //
  // NOTE: On Ash, App Service will not call this method, and instead will call
  // `GetCompressedIconData()` to load raw icon data from the Publisher.
  // TODO(crbug.com/40244797): Clean up/simplify remaining usages of LoadIcon.
  virtual void LoadIcon(const std::string& app_id,
                        const IconKey& icon_key,
                        apps::IconType icon_type,
                        int32_t size_hint_in_dip,
                        bool allow_placeholder_icon,
                        LoadIconCallback callback);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns the default icon if a valid icon can't be loaded, e.g. because an
  // app didn't supply an icon.
  virtual int DefaultIconResourceId() const;

  // CompressedIconGetter override.
  void GetCompressedIconData(const std::string& app_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             LoadIconCallback callback) override;
#endif

  // Launches an app identified by `app_id`. `event_flags` contains launch
  // options (e.g. window disposition). `launch_source` contains the source
  // of the launch. When provided, `window_info` contains the expected window
  // bounds, etc. that are requested for the placement of the launched app
  // window.
  virtual void Launch(const std::string& app_id,
                      int32_t event_flags,
                      LaunchSource launch_source,
                      WindowInfoPtr window_info) = 0;

  // DEPRECATED. Prefer passing the files in an Intent through
  // LaunchAppWithIntent.
  // TODO(crbug.com/40203246): Remove this method.
  virtual void LaunchAppWithFiles(const std::string& app_id,
                                  int32_t event_flags,
                                  LaunchSource launch_source,
                                  std::vector<base::FilePath> file_paths);

  // Launches an app with `app_id` and Chrome OS generic `intent` irrespective
  // of app platform. Returns whether the app was successfully launched.
  virtual void LaunchAppWithIntent(const std::string& app_id,
                                   int32_t event_flags,
                                   IntentPtr intent,
                                   LaunchSource launch_source,
                                   WindowInfoPtr window_info,
                                   LaunchCallback callback);

  // Launches an app with `params`.
  //
  // Publishers implementing this method should:
  // - Launch the app represent by the `params.app_id`.
  // - Launch the app with all the params that is applicable to the platform.
  // - Return launch_result if applicable.
  virtual void LaunchAppWithParams(AppLaunchParams&& params,
                                   LaunchCallback callback) = 0;

  virtual void LaunchShortcut(const std::string& app_id,
                              const std::string& shortcut_id,
                              int64_t display_id) {}

  // Sets `permission` for an app identified with `app_id`. Implemented if the
  // publisher supports per-app permissions that are exposed in App Management.
  virtual void SetPermission(const std::string& app_id,
                             PermissionPtr permission);

  // Directly uninstalls an app identified by `app_id` without prompting the
  // user. `uninstall_source` indicates where the uninstallation came from.
  // `clear_site_data` is available for web apps only. If true, any site
  // data associated with the app will be removed.
  // `report_abuse` is available for Chrome Apps only. If true, the app will be
  // reported for abuse to the Chrome Web Store.
  virtual void Uninstall(const std::string& app_id,
                         UninstallSource uninstall_source,
                         bool clear_site_data,
                         bool report_abuse);

  // Requests that the app identified by `app_id` is marked as paused. Paused
  // apps cannot be launched. Implemented if the publisher supports the pausing
  // of apps, and otherwise should do nothing.
  //
  // Publishers are expected to update the app icon when it is paused to apply
  // the kPaused icon effect. Nothing should happen if an already paused app
  // is paused again.
  virtual void PauseApp(const std::string& app_id);

  // Requests that the app identified by `app_id` is unpaused. Implemented if
  // the publisher supports the pausing of apps, and otherwise should do
  // nothing.
  //
  // Publishers are expected to update the app icon to remove the kPaused
  // icon effect. Nothing should happen if an unpaused app is unpaused again.
  virtual void UnpauseApp(const std::string& app_id);

  // Blocks the given app.
  virtual void BlockApp(const std::string& app_id);

  // Unblocks the given app.
  virtual void UnblockApp(const std::string& app_id);

  // Stops all running instances of `app_id`.
  virtual void StopApp(const std::string& app_id);

  // Returns the menu items for an app with `app_id`.
  virtual void GetMenuModel(const std::string& app_id,
                            MenuType menu_type,
                            int64_t display_id,
                            base::OnceCallback<void(MenuItems)> callback);

  // Requests the size of an app with `app_id`. Publishers are expected to
  // calculate and update the size of the app and publish this to App Service.
  // This allows app sizes to be requested on-demand and ensure up-to-date
  // values.
  virtual void UpdateAppSize(const std::string& app_id);

  // Executes the menu item command for an app with `app_id`.
  virtual void ExecuteContextMenuCommand(const std::string& app_id,
                                         int command_id,
                                         const std::string& shortcut_id,
                                         int64_t display_id);

  // Opens the platform-specific settings page for the app identified by
  // `app_id`, e.g. the Android Settings app for an ARC app, or the Chrome
  // browser settings for a web app. Implemented if those settings exist and
  // need to be accessible to users. Note this is not the same as the Chrome
  // OS-wide App Management page, which should be used by default. This method
  // should only be used in cases where settings must be accessed that are not
  // available in App Management.
  virtual void OpenNativeSettings(const std::string& app_id);

  // Indicates that the app identified by `app_id` has had its supported links
  // preference changed, so that all supported link filters are either preferred
  // (`open_in_app` is true) or not preferred (`open_in_app` is false). This
  // method is used by the App Service to sync changes to publishers, and is
  // called instead of OnPreferredAppSet for supported links changes.
  virtual void OnSupportedLinksPreferenceChanged(const std::string& app_id,
                                                 bool open_in_app) {}

  // Enables resize lock mode for the app identified by `app_id`. When `locked`
  // is kTrue, this means the app cannot be resized and is locked to a certain
  // set of dimensions. Implemented if the publisher supports resize locking of
  // apps, and otherwise should do nothing.
  virtual void SetResizeLocked(const std::string& app_id, bool locked);

  // Set the window display mode for the app identified by `app_id`. Implemented
  // if the publisher supports changing the window mode of apps, and otherwise
  // should do nothing.
  virtual void SetWindowMode(const std::string& app_id, WindowMode window_mode);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Set the locale for the app identified by `app_id`. Implemented if the
  // publisher supports changing app-specific locale, and otherwise should do
  // nothing.
  virtual void SetAppLocale(const std::string& app_id,
                            const std::string& locale_tag);

  // Creates and returns a promise app object.
  static PromiseAppPtr MakePromiseApp(const PackageId& package_id);

  // Publishes a single promise app delta to the Promise App Registry Cache.
  void PublishPromiseApp(PromiseAppPtr delta);
#endif

 protected:
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Publish one `app` to AppServiceProxy. Should be called whenever the app
  // represented by `app` undergoes some state change to inform AppServiceProxy
  // of the change. Ensure that RegisterPublisher() has been called before the
  // first call to this method.
  void Publish(AppPtr app);

  // Publish multiple `apps` to AppServiceProxy. Should be called whenever the
  // apps represented by `apps` undergoes some state change to inform
  // AppServiceProxy of the change. Ensure that RegisterPublisher() has been
  // called before the first call to this method.
  //
  // `should_notify_initialized` is true, when the publisher for `app_type` has
  // finished initiating apps - typically this is the very first time Publish()
  // is called with the initial set of apps present at the time the publisher is
  // first created. Otherwise `should_notify_initialized` is false. When
  // `should_notify_initialized` is true, `app_type` should not be `kUnknown`.
  void Publish(std::vector<AppPtr> apps,
               AppType app_type,
               bool should_notify_initialized);

  // Modifies CapabilityAccess for `app_id`.
  void ModifyCapabilityAccess(const std::string& app_id,
                              std::optional<bool> accessing_camera,
                              std::optional<bool> accessing_microphone);

  // Resets all tracked capabilities for apps of type `app_type`. Should be
  // called when the publisher stops running apps (e.g. when a VM shuts down).
  void ResetCapabilityAccess(AppType app_type);
#endif

  AppServiceProxy* proxy() { return proxy_; }

 private:
  const raw_ptr<AppServiceProxy, DanglingUntriaged> proxy_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_APP_PUBLISHER_H_
