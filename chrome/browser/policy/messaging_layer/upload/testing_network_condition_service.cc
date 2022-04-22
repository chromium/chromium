// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/testing_network_condition_service.h"

#include <cstddef>

#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"

namespace reporting {

TestingNetworkConditionService::TestingNetworkConditionService(
    content::BrowserTaskEnvironment* task_environment) {
  // Reset the unique pointer of observer will post the task to delete the
  // observer. |TestingNetworkConditionService| is better off without an
  // observer changing upload_rate_ in the way.
  observer_.reset();
  task_environment->RunUntilIdle();
}

void TestingNetworkConditionService::SetUploadRate(uint64_t upload_rate) {
  upload_rate_ = upload_rate;
}

}  // namespace reporting
