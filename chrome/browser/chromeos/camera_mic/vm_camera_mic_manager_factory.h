// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {

class VmCameraMicManager;

class VmCameraMicManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static VmCameraMicManager* GetForProfile(Profile* profile);

  static VmCameraMicManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<VmCameraMicManagerFactory>;

  VmCameraMicManagerFactory();

  ~VmCameraMicManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_FACTORY_H_
