// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_FACTORY_H_
#define CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "content/public/browser/browser_context.h"

namespace ash {

class ScalableIphFactory : public ProfileKeyedServiceFactory {
 public:
  static ScalableIphFactory* GetInstance();
  static scalable_iph::ScalableIph* GetForProfile(Profile* profile);

 protected:
  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;

 private:
  friend base::NoDestructor<ScalableIphFactory>;

  ScalableIphFactory();
  ~ScalableIphFactory() override;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_FACTORY_H_
