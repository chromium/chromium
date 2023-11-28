// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_GUEST_OS_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_GUEST_OS_APPS_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_types.h"

class Profile;

namespace apps {

class PublisherHost;

// GuestOSApps holds the common code for GuestOS app publishers (in the App
// Service sense). Subclasses like CrostiniApps and BruschettaApps should
// inherit from this.
class GuestOSApps : public KeyedService,
                    public AppPublisher,
                    public guest_os::GuestOsRegistryService::Observer {
 public:
  explicit GuestOSApps(AppServiceProxy* proxy);
  GuestOSApps(const GuestOSApps&) = delete;
  GuestOSApps& operator=(const GuestOSApps&) = delete;
  ~GuestOSApps() override;

  void InitializeForTesting();

 protected:
  // Returns false if this kind of GuestOS isn't supported, e.g. missing
  // hardware capabilities. This prevents the app publisher from being
  // registered at all.
  virtual bool CouldBeAllowed() const = 0;

  virtual apps::AppType AppType() const = 0;
  virtual guest_os::VmType VmType() const = 0;

  virtual void Initialize();

  // Returns launch args where files in the intent are converted to URLs.
  std::vector<guest_os::LaunchArg> ArgsFromIntent(const apps::Intent* intent);

  // CreateApp calls this to override App defaults with per-OS values.
  virtual void CreateAppOverrides(
      const guest_os::GuestOsRegistryService::Registration& registration,
      App* app) {}

  const raw_ptr<Profile> profile() const { return profile_; }
  const raw_ptr<guest_os::GuestOsRegistryService> registry() const {
    return registry_;
  }

 private:
  friend class PublisherHost;  // It calls Initialize().

  // apps::AppPublisher overrides.
  void GetCompressedIconData(const std::string& app_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             LoadIconCallback callback) final;
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) final;

  // GuestOsRegistryService::Observer overrides.
  void OnRegistryUpdated(guest_os::GuestOsRegistryService* registry_service,
                         guest_os::VmType vm_type,
                         const std::vector<std::string>& updated_apps,
                         const std::vector<std::string>& removed_apps,
                         const std::vector<std::string>& inserted_apps) final;
  void OnAppLastLaunchTimeUpdated(guest_os::VmType vm_type,
                                  const std::string& app_id,
                                  const base::Time& last_launch_time) override;

  AppPtr CreateApp(
      const guest_os::GuestOsRegistryService::Registration& registration,
      bool generate_new_icon_key);

  const raw_ptr<Profile> profile_;
  raw_ptr<guest_os::GuestOsRegistryService> registry_;
  base::ScopedObservation<guest_os::GuestOsRegistryService, GuestOSApps>
      registry_observation_{this};
};

// Create a file intent filter with mime type conditions for App Service.
apps::IntentFilters CreateIntentFilterForAppService(
    const guest_os::GuestOsMimeTypesService* mime_types_service,
    const guest_os::GuestOsRegistryService::Registration& registration);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_GUEST_OS_APPS_H_
