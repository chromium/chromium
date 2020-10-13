// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PEPPER_FLASH_SETTINGS_MANAGER_H_
#define CHROME_BROWSER_PEPPER_FLASH_SETTINGS_MANAGER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/c/private/ppp_flash_browser_operations.h"
#include "ppapi/shared_impl/ppp_flash_browser_operations_shared.h"

class PluginPrefs;
class PrefService;

namespace content {
class BrowserContext;
struct WebPluginInfo;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// PepperFlashSettingsManager communicates with a PPAPI broker process to
// read/write Pepper Flash settings.
class PepperFlashSettingsManager {
 public:
  class Client {
   public:
    virtual ~Client() {}

    virtual void OnDeauthorizeFlashContentLicensesCompleted(uint32_t request_id,
                                                            bool success) {}
    virtual void OnGetPermissionSettingsCompleted(
        uint32_t request_id,
        bool success,
        PP_Flash_BrowserOperations_Permission default_permission,
        const ppapi::FlashSiteSettings& sites) {}

    virtual void OnSetDefaultPermissionCompleted(uint32_t request_id,
                                                 bool success) {}

    virtual void OnSetSitePermissionCompleted(uint32_t request_id,
                                              bool success) {}

    virtual void OnGetSitesWithDataCompleted(
        uint32_t request_id,
        const std::vector<std::string>& sites) {}

    virtual void OnClearSiteDataCompleted(uint32_t request_id, bool success) {}
  };

  // |client| must outlive this object. It is guaranteed that |client| won't
  // receive any notifications after this object goes away.
  PepperFlashSettingsManager(Client* client,
                             content::BrowserContext* browser_context);
  ~PepperFlashSettingsManager();

  // |plugin_info| will be updated if it is not NULL and the method returns
  // true.
  static bool IsPepperFlashInUse(PluginPrefs* plugin_prefs,
                                 content::WebPluginInfo* plugin_info);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Requests to deauthorize content licenses.
  // Client::OnDeauthorizeFlashContentLicensesCompleted() will be called when
  // the operation is completed.
  // The return value is the same as the request ID passed into
  // Client::OnDeauthorizeFlashContentLicensesCompleted().
  uint32_t DeauthorizeContentLicenses(PrefService* prefs);

  // Gets permission settings.
  // Client::OnGetPermissionSettingsCompleted() will be called when the
  // operation is completed.
  uint32_t GetPermissionSettings(
      PP_Flash_BrowserOperations_SettingType setting_type);

  // Sets default permission.
  // Client::OnSetDefaultPermissionCompleted() will be called when the
  // operation is completed.
  uint32_t SetDefaultPermission(
      PP_Flash_BrowserOperations_SettingType setting_type,
      PP_Flash_BrowserOperations_Permission permission,
      bool clear_site_specific);

  // Sets site-specific permission.
  // Client::OnSetSitePermissionCompleted() will be called when the operation
  // is completed.
  uint32_t SetSitePermission(
      PP_Flash_BrowserOperations_SettingType setting_type,
      const ppapi::FlashSiteSettings& sites);

  // Gets a list of sites that have stored data.
  // Client::OnGetSitesWithDataCompleted() will be called when the operation is
  // completed.
  uint32_t GetSitesWithData();

  // Clears data for a certain site.
  // Client::OnClearSiteDataompleted() will be called when the operation is
  // completed.
  uint32_t ClearSiteData(const std::string& site,
                         uint64_t flags,
                         uint64_t max_age);

 private:
  // Core does most of the work. It is ref-counted so that its lifespan can be
  // independent of the containing object's:
  // - The manager can be deleted on the UI thread while the core still being
  // used on the I/O thread.
  // - The manager can delete the core when it encounters errors and create
  // another one to handle new requests.
  class Core;

  uint32_t GetNextRequestId();

  void EnsureCoreExists();

  // Notifies us that an error occurred in |core|.
  void OnError(Core* core);

  // |client_| is not owned by this object and must outlive it.
  Client* client_;

  // The browser context for the profile.
  content::BrowserContext* browser_context_;

  scoped_refptr<Core> core_;

  uint32_t next_request_id_;

  base::WeakPtrFactory<PepperFlashSettingsManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PepperFlashSettingsManager);
};

#endif  // CHROME_BROWSER_PEPPER_FLASH_SETTINGS_MANAGER_H_
