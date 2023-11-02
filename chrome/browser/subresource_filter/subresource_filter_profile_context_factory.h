// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_PROFILE_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_PROFILE_CONTEXT_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace subresource_filter {
class SubresourceFilterProfileContext;
}

// This class is responsible for instantiating a profile-scoped context for
// subresource filtering.
class SubresourceFilterProfileContextFactory
    : public ProfileKeyedServiceFactory {
 public:
  static subresource_filter::SubresourceFilterProfileContext* GetForProfile(
      Profile* profile);

  static SubresourceFilterProfileContextFactory* GetInstance();

  SubresourceFilterProfileContextFactory();

  SubresourceFilterProfileContextFactory(
      const SubresourceFilterProfileContextFactory&) = delete;
  SubresourceFilterProfileContextFactory& operator=(
      const SubresourceFilterProfileContextFactory&) = delete;

 private:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_PROFILE_CONTEXT_FACTORY_H_
