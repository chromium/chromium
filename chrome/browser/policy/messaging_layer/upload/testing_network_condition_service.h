// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_TESTING_NETWORK_CONDITION_SERVICE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_TESTING_NETWORK_CONDITION_SERVICE_H_

#include <cstddef>

#include "base/time/time.h"
#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"
#include "content/public/test/browser_task_environment.h"

namespace reporting {
// Utilities for using network condition service in a test environment. The main
// purpose of this class is to provide access to private members of
// |NetworkConditionService| without exposing them in the production code and
// remove code that would update its internal upload_rate_. Also unsubscribe
// from network condition change notified by
// g_browser_process->network_quality_tracker() in the constructor to avoid
// unexpected change in upload_rate_.
//
// Example Usage:
//
//   TestingNetworkConditionService network_condition_service;
//   network_condition_service.SetUploadRate(1000);
//
class TestingNetworkConditionService : public NetworkConditionService {
 public:
  // Unsubscribe from g_browser_process->network_quality_tracker() via the UI
  // thread and clear the task queue on the UI thread immediately.
  explicit TestingNetworkConditionService(
      content::BrowserTaskEnvironment* task_environment);

  // Set the private variable upload_rate_ directly. This is what
  // |GetUploadRate| returns.
  void SetUploadRate(uint64_t upload_rate);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_TESTING_NETWORK_CONDITION_SERVICE_H_
