// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/test_request_coordinator_builder.h"

#include <memory>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/offliner_stub.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_queue.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/scheduler_stub.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "content/public/browser/browser_context.h"

namespace offline_pages {

namespace {
class ActiveTabInfo : public RequestCoordinator::ActiveTabInfo {
 public:
  ~ActiveTabInfo() override {}
  bool DoesActiveTabMatch(const GURL&) override { return false; }
};
}  // namespace

std::unique_ptr<KeyedService> BuildTestRequestCoordinator(
    content::BrowserContext* context) {
  // Use original policy.
  std::unique_ptr<OfflinerPolicy> policy(new OfflinerPolicy());

  // Use the regular test queue (should work).
  std::unique_ptr<RequestQueue> queue(
      new RequestQueue(std::make_unique<TestRequestQueueStore>()));

  // Initialize the rest with stubs.
  std::unique_ptr<Offliner> offliner(new OfflinerStub());
  std::unique_ptr<Scheduler> scheduler_stub(new SchedulerStub());

  return std::make_unique<RequestCoordinator>(
      std::move(policy), std::move(offliner), std::move(queue),
      std::move(scheduler_stub), g_browser_process->network_quality_tracker(),
      std::make_unique<ActiveTabInfo>());
}

}  // namespace offline_pages
