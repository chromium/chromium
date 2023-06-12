// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCALABLE_IPH_SCALABLE_IPH_FACTORY_H_
#define CHROME_BROWSER_SCALABLE_IPH_SCALABLE_IPH_FACTORY_H_

#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

class ScalableIphFactory : public BrowserContextKeyedServiceFactory {
 public:
  using DelegateTestingFactory = base::RepeatingCallback<
      std::unique_ptr<scalable_iph::ScalableIphDelegate>()>;

  static ScalableIphFactory* GetInstance();
  static scalable_iph::ScalableIph* GetForProfile(Profile* profile);

  void SetDelegateFactoryForTesting(
      DelegateTestingFactory delegate_testing_factory);

  bool has_delegate_factory_for_testing() const {
    return !delegate_testing_factory_.is_null();
  }

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
