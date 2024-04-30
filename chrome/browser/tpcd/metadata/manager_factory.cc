// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/metadata/manager_factory.h"

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/tpcd/metadata/browser/manager.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/features.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace tpcd::metadata {

// static
Manager* ManagerFactory::GetForProfile(Profile* profile) {
  if (!base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants)) {
    return nullptr;
  }

  if (!(profile->IsRegularProfile() || profile->IsGuestSession())) {
    return nullptr;
  }

  auto sync_network_service = [](const ContentSettingsForOneType& grants) {
    content::GetNetworkService()->SetTpcdMetadataGrants(grants);
  };

  return tpcd::metadata::Manager::GetInstance(
      Parser::GetInstance(), base::BindRepeating(sync_network_service),
      g_browser_process->local_state());
}

}  // namespace tpcd::metadata
