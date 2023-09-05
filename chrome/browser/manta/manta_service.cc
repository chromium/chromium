// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manta/manta_service.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/manta/orca_provider.h"
#include "chrome/browser/manta/snapper_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"

namespace manta {

MantaService::MantaService(Profile* const profile) : profile_(profile) {
  CHECK(profile_);
}

std::unique_ptr<OrcaProvider> MantaService::CreateOrcaProvider() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CHECK(identity_manager);

  return std::make_unique<OrcaProvider>(
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      identity_manager);
}

std::unique_ptr<SnapperProvider> MantaService::CreateSnapperProvider() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CHECK(identity_manager);

  return std::make_unique<SnapperProvider>(
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      identity_manager);
}

}  // namespace manta
