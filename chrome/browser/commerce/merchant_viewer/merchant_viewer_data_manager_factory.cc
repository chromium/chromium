// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/merchant_viewer/merchant_viewer_data_manager_factory.h"

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/commerce/merchant_viewer/merchant_signal_db.h"
#include "chrome/browser/commerce/merchant_viewer/merchant_viewer_data_manager.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/android/browser_context_handle.h"

// static
MerchantViewerDataManager* MerchantViewerDataManagerFactory::GetForProfile(
    Profile* profile) {
  if (!profile) {
    return nullptr;
  }

  // We'll try to create a new instance regardless of whether or not the feature
  // is enabled.
  return static_cast<MerchantViewerDataManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MerchantViewerDataManagerFactory*
MerchantViewerDataManagerFactory::GetInstance() {
  static base::NoDestructor<MerchantViewerDataManagerFactory> factory;
  return factory.get();
}

MerchantViewerDataManagerFactory::MerchantViewerDataManagerFactory()
    : ProfileKeyedServiceFactory(
          "MerchantViewerDataManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(SessionProtoDBFactory<
            MerchantViewerDataManager::MerchantSignalProto>::GetInstance());
}

MerchantViewerDataManagerFactory::~MerchantViewerDataManagerFactory() = default;

std::unique_ptr<KeyedService>
MerchantViewerDataManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<MerchantViewerDataManager>(context);
}
