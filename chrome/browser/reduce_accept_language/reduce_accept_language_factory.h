// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_FACTORY_H_
#define CHROME_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/reduce_accept_language/browser/reduce_accept_language_service.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"

class Profile;

class ReduceAcceptLanguageFactory : public ProfileKeyedServiceFactory {
 public:
  static reduce_accept_language::ReduceAcceptLanguageService* GetForProfile(
      Profile* profile);

  static ReduceAcceptLanguageFactory* GetInstance();

  // Non-copyable, non-moveable.
  ReduceAcceptLanguageFactory(const ReduceAcceptLanguageFactory&) = delete;
  ReduceAcceptLanguageFactory& operator=(const ReduceAcceptLanguageFactory&) =
      delete;

 private:
  friend base::NoDestructor<ReduceAcceptLanguageFactory>;

  ReduceAcceptLanguageFactory();
  ~ReduceAcceptLanguageFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_FACTORY_H_
