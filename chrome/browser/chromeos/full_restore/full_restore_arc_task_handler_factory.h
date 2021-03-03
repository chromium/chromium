// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_ARC_TASK_HANDLER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_ARC_TASK_HANDLER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {
namespace full_restore {

class FullRestoreArcTaskHandler;

class FullRestoreArcTaskHandlerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static FullRestoreArcTaskHandler* GetForProfile(Profile* profile);

  static FullRestoreArcTaskHandlerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<FullRestoreArcTaskHandlerFactory>;

  FullRestoreArcTaskHandlerFactory();
  FullRestoreArcTaskHandlerFactory(const FullRestoreArcTaskHandlerFactory&) =
      delete;
  FullRestoreArcTaskHandlerFactory& operator=(
      const FullRestoreArcTaskHandlerFactory&) = delete;
  ~FullRestoreArcTaskHandlerFactory() override = default;

  // BrowserContextKeyedServiceFactory.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_ARC_TASK_HANDLER_FACTORY_H_
