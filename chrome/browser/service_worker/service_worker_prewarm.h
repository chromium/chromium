// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PREWARM_H_
#define CHROME_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PREWARM_H_

#include <optional>

namespace content {
class BrowserContext;
class ServiceWorkerContext;
}  // namespace content

namespace chrome_service_worker {

void PrewarmServiceWorkerRegistrationForDSE(
    content::BrowserContext* browser_context,
    content::ServiceWorkerContext& service_worker_context);

std::optional<int>&
PrewarmServiceWorkerRegistrationForDSECalledCountForTesting();

}  // namespace chrome_service_worker

#endif  // CHROME_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PREWARM_H_
