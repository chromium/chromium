// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/cloud_print.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PRINT_PREVIEW) || defined(OS_CHROMEOS)
#error "Print Preview must be enabled / Not supported on ChromeOS"
#endif

class Profile;
class ServiceProcessControl;

// Layer between the browser user interface and the cloud print proxy code
// running in the service process.
class CloudPrintProxyService : public KeyedService {
 public:
  explicit CloudPrintProxyService(Profile* profile);
  ~CloudPrintProxyService() override;

  using PrintersCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize();

  // Returns list of printer names available for registration.
  void GetPrinters(PrintersCallback callback);

  // Enables/disables cloud printing for the user
  virtual void EnableForUserWithRobot(const std::string& robot_auth_code,
                                      const std::string& robot_email,
                                      const std::string& user_email,
                                      base::Value user_settings);
  virtual void DisableForUser();

  // Query the service process for the status of the cloud print proxy and
  // update the browser prefs.
  void RefreshStatusFromService();

  const std::string& proxy_id() const { return proxy_id_; }

 private:
  // NotificationDelegate implementation for the token expired notification.
  class TokenExpiredNotificationDelegate;
  friend class TokenExpiredNotificationDelegate;

  // Methods that send an IPC to the service.
  void GetCloudPrintProxyPrinters(PrintersCallback callback);
  void RefreshCloudPrintProxyStatus();
  void EnableCloudPrintProxyWithRobot(const std::string& robot_auth_code,
                                      const std::string& robot_email,
                                      const std::string& user_email,
                                      base::Value user_preferences);
  void DisableCloudPrintProxy();

  // Callback that gets the cloud print proxy info.
  void ProxyInfoCallback(bool enabled,
                         const std::string& email,
                         const std::string& proxy_id);

  // Invoke a task that gets run after the service process successfully
  // launches. The task typically involves sending an IPC to the service
  // process.
  bool InvokeServiceTask(base::OnceClosure task);

  // Checks the policy. Returns true if nothing needs to be done (the policy is
  // not set or the connector is not enabled).
  bool ApplyCloudPrintConnectorPolicy();

  cloud_print::mojom::CloudPrint& GetCloudPrintProxy();

  // Virtual for testing.
  virtual ServiceProcessControl* GetServiceProcessControl();

  void OnReadCloudPrintSetupProxyList(PrintersCallback callback,
                                      const std::string& printers_json);

  Profile* const profile_;
  std::string proxy_id_;

  // For watching for connector policy changes.
  PrefChangeRegistrar pref_change_registrar_;

  mojo::Remote<cloud_print::mojom::CloudPrint> cloud_print_proxy_;

  base::WeakPtrFactory<CloudPrintProxyService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxyService);
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_
