// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace on_device_translation {

class OnDeviceTranslationServiceController;
class ServiceControllerManagerFactory;

// Manages the OnDeviceTranslationServiceControllers for a BrowserContext.
// This class is responsible for creating the per origin
// OnDeviceTranslationServiceController.
class ServiceControllerManager : public KeyedService {
 public:
  explicit ServiceControllerManager(
      base::PassKey<ServiceControllerManagerFactory>);
  ~ServiceControllerManager() override;

  ServiceControllerManager(const ServiceControllerManager&) = delete;
  ServiceControllerManager& operator=(const ServiceControllerManager&) = delete;

  // Returns the ServiceControllerManager for the specified BrowserContext. This
  // function creates the ServiceControllerManager if it hasn't been created
  // already.
  static ServiceControllerManager* GetForBrowserContext(
      content::BrowserContext* browser_context);

  scoped_refptr<OnDeviceTranslationServiceController>
  GetServiceControllerForOrigin(const url::Origin& origin);

  // Returns true if a new service can be started.
  bool CanStartNewService() const;

  // Called when a service controller is deleted.
  void OnServiceControllerDeleted(
      const url::Origin& origin,
      base::PassKey<OnDeviceTranslationServiceController>);

  // Sets the service controller deleted observer for testing.
  void set_service_controller_deleted_observer_for_testing(
      base::OnceClosure observer) {
    service_controller_deleted_observer_for_testing_ = std::move(observer);
  }

 private:
  std::map<url::Origin, raw_ptr<OnDeviceTranslationServiceController>>
      service_controllers_;
  base::OnceClosure service_controller_deleted_observer_for_testing_;
};

}  // namespace on_device_translation
#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_
