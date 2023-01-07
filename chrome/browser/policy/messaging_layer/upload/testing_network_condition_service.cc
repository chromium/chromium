// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/testing_network_condition_service.h"

#include <cstddef>

#include "base/functional/bind.h"
#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"

namespace reporting {

TestingNetworkConditionService::TestingNetworkConditionService(
    content::BrowserTaskEnvironment* task_environment) {
  // Post the task to remove the observer so that
  // g_browser_process->network_quality_tracker() won't accidentally get in the
  // way in our manipulation of artificial upload rate.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](NetworkConditionServiceImpl* network_condition_service_impl) {
            g_browser_process->network_quality_tracker()
                ->RemoveRTTAndThroughputEstimatesObserver(
                    network_condition_service_impl);
          },
          base::Unretained(impl_.get())));
  task_environment->RunUntilIdle();
}

void TestingNetworkConditionService::SetUploadRate(uint64_t upload_rate) {
  impl_->upload_rate_ = upload_rate;
}

}  // namespace reporting
