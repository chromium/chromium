// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_VPN_PROVIDER_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_VPN_PROVIDER_MANAGER_H_

// Helper class to create VPN provider specific events to observers.

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/core/keyed_service.h"

namespace app_list {

class ArcVpnProviderManager : public ArcAppListPrefs::Observer,
                              public KeyedService {
 public:
  struct ArcVpnProvider {
    ArcVpnProvider(const std::string& app_name,
                   const std::string& package_name,
                   const std::string& app_id,
                   const base::Time last_launch_time);
    ~ArcVpnProvider();

    const std::string app_name;
    const std::string package_name;
    const std::string app_id;
    const base::Time last_launch_time;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Notifies initial refresh of Arc VPN providers.
    virtual void OnArcVpnProvidersRefreshed(
        const std::vector<std::unique_ptr<ArcVpnProvider>>& arc_vpn_providers) {
    }
    // Notifies removal of an Arc VPN provider.
    virtual void OnArcVpnProviderRemoved(const std::string& package_name) {}
    // Notifies update for an Arc VPN provider. Update includes newly
    // installation, name update, launch time update.
    virtual void OnArcVpnProviderUpdated(ArcVpnProvider* arc_vpn_provider) {}

   protected:
    ~Observer() override;
  };

  static ArcVpnProviderManager* Get(content::BrowserContext* context);

  static std::unique_ptr<ArcVpnProviderManager> Create(
      content::BrowserContext* context);

  explicit ArcVpnProviderManager(ArcAppListPrefs* arc_app_list_prefs);
  ArcVpnProviderManager(const ArcVpnProviderManager&) = delete;
  ArcVpnProviderManager& operator=(const ArcVpnProviderManager&) = delete;

  ~ArcVpnProviderManager() override;

  // ArcAppListPrefs Observer:
  void OnAppNameUpdated(const std::string& id,
                        const std::string& name) override;
  void OnAppLastLaunchTimeUpdated(const std::string& app_id) override;
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  std::vector<std::unique_ptr<ArcVpnProvider>> GetArcVpnProviders();

 private:
  void MaybeNotifyArcVpnProviderUpdate(const std::string& app_id);

  const raw_ptr<ArcAppListPrefs> arc_app_list_prefs_;

  // List of observers.
  base::ObserverList<Observer> observer_list_;
};

}  // namespace app_list

#endif  //  CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_VPN_PROVIDER_MANAGER_H_
