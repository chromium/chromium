// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_IN_PROCESS_SERVICE_FACTORY_FACTORY_H_
#define CHROME_BROWSER_PREFS_IN_PROCESS_SERVICE_FACTORY_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace prefs {
class InProcessPrefServiceFactory;
}

class InProcessPrefServiceFactoryFactory : public SimpleKeyedServiceFactory {
 public:
  static InProcessPrefServiceFactoryFactory* GetInstance();

  static prefs::InProcessPrefServiceFactory* GetInstanceForKey(
      SimpleFactoryKey* key);

 private:
  friend struct base::DefaultSingletonTraits<
      InProcessPrefServiceFactoryFactory>;

  InProcessPrefServiceFactoryFactory();
  ~InProcessPrefServiceFactoryFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;

  DISALLOW_COPY_AND_ASSIGN(InProcessPrefServiceFactoryFactory);
};

#endif  // CHROME_BROWSER_PREFS_IN_PROCESS_SERVICE_FACTORY_FACTORY_H_
