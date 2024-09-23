// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_MESSAGE_HANDLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/sharing_message/sharing_message_handler.h"

class OptimizationGuideLogger;
class Profile;

namespace optimization_guide {
class PushNotificationManager;
}  // namespace optimization_guide

// Message handler to handle optimization guide push notification message from
// sharing message.
class OptimizationGuideMessageHandler : public SharingMessageHandler {
 public:
  static std::unique_ptr<OptimizationGuideMessageHandler> Create(
      Profile* profile);

  explicit OptimizationGuideMessageHandler(
      optimization_guide::PushNotificationManager* push_notification_manager,
      OptimizationGuideLogger* optimization_guide_logger);
  OptimizationGuideMessageHandler(const OptimizationGuideMessageHandler&) =
      delete;
  OptimizationGuideMessageHandler& operator=(
      const OptimizationGuideMessageHandler&) = delete;
  ~OptimizationGuideMessageHandler() override;

  // SharingMessageHandler implementation.
  void OnMessage(components_sharing_message::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 private:
  // Owned by OptimizationGuideKeyedService, must outlive this class. Can be
  // nullptr.
  raw_ptr<optimization_guide::PushNotificationManager>
      push_notification_manager_;

  // Owned by OptimizationGuideKeyedService, must outlive this class. Can be
  // nullptr.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
};

#endif  // CHROME_BROWSER_SHARING_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_MESSAGE_HANDLER_H_
