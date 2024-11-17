// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/mock_notification_content_detection_service.h"

#include "base/memory/ptr_util.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/safe_browsing/content/browser/notification_content_detection/test_model_observer_tracker.h"

namespace safe_browsing {

MockNotificationContentDetectionService::
    MockNotificationContentDetectionService(
        optimization_guide::OptimizationGuideModelProvider* model_provider,
        scoped_refptr<base::SequencedTaskRunner> background_task_runner,
        scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
            database_manager,
        content::BrowserContext* browser_context)
    : NotificationContentDetectionService(model_provider,
                                          background_task_runner,
                                          database_manager,
                                          browser_context) {}
MockNotificationContentDetectionService::
    ~MockNotificationContentDetectionService() = default;

// static
std::unique_ptr<KeyedService>
MockNotificationContentDetectionService::FactoryForTests(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    content::BrowserContext* context) {
  return base::WrapUnique(new MockNotificationContentDetectionService(
      model_provider, background_task_runner, nullptr, context));
}

}  // namespace safe_browsing
