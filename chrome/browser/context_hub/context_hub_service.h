// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_
#define CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace context_hub {

class ContextHubService : public KeyedService {
 public:
  ContextHubService();

  ContextHubService(const ContextHubService&) = delete;
  ContextHubService& operator=(const ContextHubService&) = delete;
  ~ContextHubService() override;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_
