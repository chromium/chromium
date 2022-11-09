// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_HUB_SHARING_HUB_SERVICE_H_
#define CHROME_BROWSER_SHARING_HUB_SHARING_HUB_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace sharing_hub {

class SharingHubModel;

// KeyedService responsible for the cache of Sharing Hub actions.
class SharingHubService : public KeyedService {
 public:
  explicit SharingHubService(content::BrowserContext* context);

  SharingHubService(const SharingHubService&) = delete;
  SharingHubService& operator=(const SharingHubService&) = delete;

  ~SharingHubService() override;

  virtual SharingHubModel* GetSharingHubModel();

 private:
  std::unique_ptr<SharingHubModel> model_;
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_SHARING_HUB_SHARING_HUB_SERVICE_H_
