// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_CROSTINI_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_CROSTINI_APPS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefChangeRegistrar;
class Profile;

namespace apps {

// An app publisher (in the App Service sense) of Crostini apps,
//
// See chrome/services/app_service/README.md.
class CrostiniApps : public KeyedService,
                     public apps::mojom::Publisher,
                     public crostini::CrostiniRegistryService::Observer {
 public:
  CrostiniApps(const mojo::Remote<apps::mojom::AppService>& app_service,
               Profile* profile);
  ~CrostiniApps() override;

  void FlushMojoCallsForTesting();

  void ReInitializeForTesting(
      const mojo::Remote<apps::mojom::AppService>& app_service,
      Profile* profile);

 private:
  enum class PublishAppIDType {
    kInstall,
    kUninstall,
    kUpdate,
  };

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

  // CrostiniRegistryService::Observer overrides.
  void OnRegistryUpdated(
      crostini::CrostiniRegistryService* registry_service,
      const std::vector<std::string>& updated_apps,
      const std::vector<std::string>& removed_apps,
      const std::vector<std::string>& inserted_apps) override;
  void OnAppIconUpdated(const std::string& app_id,
                        ui::ScaleFactor scale_factor) override;

  void OnCrostiniEnabledChanged();

  void LoadIconFromVM(const std::string app_id,
                      apps::mojom::IconCompression icon_compression,
                      int32_t size_hint_in_dip,
                      bool allow_placeholder_icon,
                      ui::ScaleFactor scale_factor,
                      IconEffects icon_effects,
                      LoadIconCallback callback);

  apps::mojom::AppPtr Convert(
      const std::string& app_id,
      const crostini::CrostiniRegistryService::Registration& registration,
      bool new_icon_key);
  apps::mojom::IconKeyPtr NewIconKey(const std::string& app_id);
  void PublishAppID(const std::string& app_id, PublishAppIDType type);
  void Publish(apps::mojom::AppPtr app);

  mojo::Receiver<apps::mojom::Publisher> receiver_{this};
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* profile_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  crostini::CrostiniRegistryService* registry_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  bool crostini_enabled_;

  base::WeakPtrFactory<CrostiniApps> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniApps);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_CROSTINI_APPS_H_
