// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCALABLE_IPH_SCALABLE_IPH_FACTORY_H_
#define CHROME_BROWSER_SCALABLE_IPH_SCALABLE_IPH_FACTORY_H_

#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

// Use CHECK instead of DCHECK if a constraint is coming from client side. We
// release this feature via channel based release. Those CHECKs should be caught
// during the process. Note that DCHECK and a fail-safe behavior should be
// used/implemented if a constraint is coming from server side or a config.
class ScalableIphFactory : public BrowserContextKeyedServiceFactory {
 public:
  using DelegateTestingFactory = base::RepeatingCallback<
      std::unique_ptr<scalable_iph::ScalableIphDelegate>(Profile*)>;

  static ScalableIphFactory* GetInstance();
  static scalable_iph::ScalableIph* GetForBrowserContext(
      content::BrowserContext* browser_context);

  void SetDelegateFactoryForTesting(
      DelegateTestingFactory delegate_testing_factory);

  bool has_delegate_factory_for_testing() const {
    return !delegate_testing_factory_.is_null();
  }

  // `ScalableIph` service has a repeating timer in it to invoke time tick
  // events. We want to start this service after a user login (but not during
  // OOBE session). A service must be created via this method to make sure it
  // happen. `GetForBrowserContext` does NOT instantiate a service.
  void InitializeServiceForProfile(Profile* profile);

 protected:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;

 private:
  friend base::NoDestructor<ScalableIphFactory>;

  std::unique_ptr<scalable_iph::ScalableIphDelegate> CreateScalableIphDelegate(
      Profile* profile) const;

  ScalableIphFactory();
  ~ScalableIphFactory() override;

  DelegateTestingFactory delegate_testing_factory_;
};

#endif  // CHROME_BROWSER_SCALABLE_IPH_SCALABLE_IPH_FACTORY_H_
