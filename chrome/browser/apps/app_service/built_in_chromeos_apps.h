// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BUILT_IN_CHROMEOS_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BUILT_IN_CHROMEOS_APPS_H_

#include <string>

#include "base/macros.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace apps {

// An app publisher (in the App Service sense) of built-in Chrome OS apps.
//
// See chrome/services/app_service/README.md.
class BuiltInChromeOsApps : public apps::mojom::Publisher {
 public:
  BuiltInChromeOsApps(const mojo::Remote<apps::mojom::AppService>& app_service,
                      Profile* profile);
  ~BuiltInChromeOsApps() override;

  void FlushMojoCallsForTesting();

  static bool SetHideSettingsAppForTesting(bool hide);

 private:
  void Initialize(const mojo::Remote<apps::mojom::AppService>& app_service);

  // apps::mojom::Publisher overrides.
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void PromptUninstall(const std::string& app_id) override;
  void Uninstall(const std::string& app_id,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApps(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void OnPreferredAppSet(const std::string& app_id,
                         apps::mojom::IntentFilterPtr intent_filter,
                         apps::mojom::IntentPtr intent) override;

  mojo::Receiver<apps::mojom::Publisher> receiver_{this};

  Profile* profile_;

  // Hack to hide the settings app from the app list search box. This is only
  // intended to be used in tests.
  //
  // BuiltInChromeOsApps assumes that the list of internal apps doesn't change
  // over the lifetime of the Profile. For example,
  // chrome/browser/ui/app_list/internal_app/internal_app_metadata.{cc,h}
  // doesn't expose any observer mechanism to be notified if it did change.
  //
  // Separately, WebAppUiServiceMigrationBrowserTest's
  // SettingsSystemWebAppMigration test exercises the case where the
  // *implementation* of the built-in Settings app changes, from an older
  // technology to being a Web App. Even though there are two implementations,
  // any given Profile will typically use only one, depending on whether
  // features::kSystemWebApps is enabled, typically via the command line.
  // Again, the App Service's BuiltInChromeOsApps handles this, in practice, as
  // whether or not it's enabled ought to stay unchanged throughout a session.
  //
  // Specifically, though, that test (1) starts with features::kSystemWebApps
  // disabled (during SetUp) but then (2) enables it (during the test
  // function). This isn't in order to explicitly exercise a run-time flag
  // change, but it was simply an expedient way to test data migration:
  // situation (1) creates some persistent data (in the test's temporary
  // storage) in the old implementation's format, and flipping to situation (2)
  // checks that the new implementation correctly migrates that data.
  //
  // An unfortunate side effect of that flip is that the list of internal apps
  // changed. The SettingsSystemWebAppMigration test involves an
  // AppListModelUpdater, which calls into its AppSearchProvider's and thus the
  // App Service (when the App Service is enabled), which then panics because a
  // built-in app it previously knew about and exposed via app list search, in
  // situation (1), has mysteriously vanished, in situation (2).
  //
  // A 'proper' fix could be to add an observer mechanism to the list of
  // internal apps, and break the previously held simplifying assumption that
  // BuiltInChromeOsApps can be relatively stateless as that list does not
  // change. There's also some subtlety because App Service methods are
  // generally asynchronous (because they're potentially Mojo IPC), so some
  // care still needs to be taken where some parts of the system might have
  // seen any given app update but others might not yet have.
  //
  // Doing a 'proper' fix is probably more trouble than it's worth. Given that
  // this "change the implementation of the built-in Settings app" will only
  // temporarily have two separate implementations, say for a couple of months,
  // and a runtime flag flip isn't supported in production, and is only
  // exercised in a single test, the simpler (temporary) fix is to opt in to
  // hiding the internal Settings app from search.
  //
  // Note that we only hide it from search. We don't remove the app entirely
  // from the list of apps that the BuiltInChromeOsApps publishes, since the
  // migration test (and its AppListModelUpdater) still needs to be able to
  // refer to the (artificial) situation where both apps co-exist.
  //
  // TODO(calamity/nigeltao): remove this hack once the System Web App has
  // completely replaced the Settings Internal App, hopefully some time in
  // 2019Q3.
  //
  // See also the "web_app::SystemWebAppManager::IsEnabled()" if-check in
  // internal_app_metadata.cc. Once that condition is removed, we can probably
  // remove this hack too.
  static bool hide_settings_app_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(BuiltInChromeOsApps);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BUILT_IN_CHROMEOS_APPS_H_
