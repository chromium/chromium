// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PORT_FORWARDER_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PORT_FORWARDER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace crostini {

class CrostiniPortForwarder;

class CrostiniPortForwarderFactory : public ProfileKeyedServiceFactory {
 public:
  static CrostiniPortForwarder* GetForProfile(Profile* profile);

  static CrostiniPortForwarderFactory* GetInstance();

  CrostiniPortForwarderFactory(const CrostiniPortForwarderFactory&) = delete;
  CrostiniPortForwarderFactory& operator=(const CrostiniPortForwarderFactory&) =
      delete;

 private:
  friend class base::NoDestructor<CrostiniPortForwarderFactory>;

  CrostiniPortForwarderFactory();
  ~CrostiniPortForwarderFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PORT_FORWARDER_FACTORY_H_
