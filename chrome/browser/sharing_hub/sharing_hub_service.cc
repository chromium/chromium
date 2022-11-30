// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing_hub/sharing_hub_service.h"

#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "content/public/browser/browser_context.h"

namespace sharing_hub {

SharingHubService::SharingHubService(content::BrowserContext* context) {
  model_ = std::make_unique<SharingHubModel>(context);
}

SharingHubService::~SharingHubService() = default;

SharingHubModel* SharingHubService::GetSharingHubModel() {
  return model_.get();
}

}  // namespace sharing_hub
