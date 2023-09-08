// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/read_write_cards/read_write_cards_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace chromeos {

class ReadWriteCardsFactory : public ProfileKeyedServiceFactory {
 public:
  static ReadWriteCardsFactory* GetInstance();
  static ReadWriteCardsManager* GetForBrowserContext(
      content::BrowserContext* browser_context);

 protected:
  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;

 private:
  friend base::NoDestructor<ReadWriteCardsFactory>;

  ReadWriteCardsFactory();
  ~ReadWriteCardsFactory() override;
};

}  //  namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_FACTORY_H_
