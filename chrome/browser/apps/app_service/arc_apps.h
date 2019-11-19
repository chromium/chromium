// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_ARC_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_ARC_APPS_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/arc_icon_once_loader.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace apps {

class AppServiceProxy;

// An app publisher (in the App Service sense) of ARC++ apps,
//
// See chrome/services/app_service/README.md.
class ArcApps : public KeyedService,
                public apps::mojom::Publisher,
                public ArcAppListPrefs::Observer,
                public arc::ArcIntentHelperObserver {
 public:
  static ArcApps* Get(Profile* profile);

  static ArcApps* CreateForTesting(Profile* profile,
                                   apps::AppServiceProxy* proxy);

  explicit ArcApps(Profile* profile);

  ~ArcApps() override;

 private:
  ArcApps(Profile* profile, apps::AppServiceProxy* proxy);

  // KeyedService overrides.
  void Shutdown() override;

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

  // ArcAppListPrefs::Observer overrides.
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& app_id) override;
  void OnAppIconUpdated(const std::string& app_id,
                        const ArcAppIconDescriptor& descriptor) override;
  void OnAppNameUpdated(const std::string& app_id,
                        const std::string& name) override;
  void OnAppLastLaunchTimeUpdated(const std::string& app_id) override;
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageModified(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageListInitialRefreshed() override;

  // arc::ArcIntentHelperObserver overrides.
  void OnIntentFiltersUpdated(
      const base::Optional<std::string>& package_name) override;

  void LoadPlayStoreIcon(apps::mojom::IconCompression icon_compression,
                         int32_t size_hint_in_dip,
                         IconEffects icon_effects,
                         LoadIconCallback callback);

  apps::mojom::AppPtr Convert(ArcAppListPrefs* prefs,
                              const std::string& app_id,
                              const ArcAppListPrefs::AppInfo& app_info,
                              bool update_icon = true);
  void Publish(apps::mojom::AppPtr app);
  void ConvertAndPublishPackageApps(
      const arc::mojom::ArcPackageInfo& package_info,
      bool update_icon = true);
  void SetIconEffect(const std::string& app_id);
  void UpdateAppIntentFilters(
      std::string package_name,
      arc::ArcIntentHelperBridge* intent_helper_bridge,
      std::vector<apps::mojom::IntentFilterPtr>* intent_filters);

  mojo::Receiver<apps::mojom::Publisher> receiver_{this};
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* profile_;
  ArcIconOnceLoader arc_icon_once_loader_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  std::set<std::string> paused_apps_;

  ScopedObserver<arc::ArcIntentHelperBridge, arc::ArcIntentHelperObserver>
      arc_intent_helper_observer_{this};

  base::WeakPtrFactory<ArcApps> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcApps);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_ARC_APPS_H_
