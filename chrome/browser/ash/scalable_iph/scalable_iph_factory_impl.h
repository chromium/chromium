// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_FACTORY_IMPL_H_
#define CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_FACTORY_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/scalable_iph/logger.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"

// `ScalableIphFactoryImpl` is responsible for initializing `ScalableIphFactory`
// instances. This is to circumvent dependencies on //chrome/browser,
// specifically the `TrackerFactory` dependency.
class ScalableIphFactoryImpl : public ScalableIphFactory {
 public:
  using DelegateTestingFactory = base::RepeatingCallback<std::unique_ptr<
      scalable_iph::ScalableIphDelegate>(Profile*, scalable_iph::Logger*)>;

  ScalableIphFactoryImpl();
  ~ScalableIphFactoryImpl() override;

  static void BuildInstance();

  static bool IsBrowserContextEligible(
      content::BrowserContext* browser_context);

  void SetDelegateFactoryForTesting(
      DelegateTestingFactory delegate_testing_factory);

  bool has_delegate_factory_for_testing() const {
    return !delegate_testing_factory_.is_null();
  }

  content::BrowserContext* GetBrowserContextToUseForDebug(
      content::BrowserContext* browser_context,
      scalable_iph::Logger* logger) const override;

 protected:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;

 private:
  friend base::NoDestructor<ScalableIphFactoryImpl>;

  // This is the actual implementation of `GetBrowserContextToUse`. We have this
  // interface to have logging. `GetBrowserContextToUse` is a const member
  // function. We have to pass a logger from outside.
  content::BrowserContext* GetBrowserContextToUseInternal(
      content::BrowserContext* browser_context,
      scalable_iph::Logger* logger) const;

  std::unique_ptr<scalable_iph::ScalableIphDelegate> CreateScalableIphDelegate(
      Profile* profile,
      scalable_iph::Logger* logger) const;

  DelegateTestingFactory delegate_testing_factory_;
};

#endif  // CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_FACTORY_IMPL_H_
