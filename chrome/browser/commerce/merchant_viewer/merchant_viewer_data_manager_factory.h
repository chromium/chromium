// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_MERCHANT_VIEWER_MERCHANT_VIEWER_DATA_MANAGER_FACTORY_H_
#define CHROME_BROWSER_COMMERCE_MERCHANT_VIEWER_MERCHANT_VIEWER_DATA_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/commerce/merchant_viewer/merchant_viewer_data_manager.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace content {
class BrowserContext;
}  // namespace content

class MerchantViewerDataManager;
class Profile;

// LazyInstance that owns all MerchantViewerDataManager(s) and associates them
// with Profiles.
class MerchantViewerDataManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the MerchantViewerDataManager for the profile.
  static MerchantViewerDataManager* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all MerchantViewerDataManager(s).
  static MerchantViewerDataManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<MerchantViewerDataManagerFactory>;

  MerchantViewerDataManagerFactory();
  ~MerchantViewerDataManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_COMMERCE_MERCHANT_VIEWER_MERCHANT_VIEWER_DATA_MANAGER_FACTORY_H_
