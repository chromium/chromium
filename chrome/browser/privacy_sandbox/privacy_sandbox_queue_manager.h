// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_QUEUE_MANAGER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_QUEUE_MANAGER_H_

#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt_helper.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/user_education/common/product_messaging_controller.h"

DECLARE_REQUIRED_NOTICE_IDENTIFIER(kPrivacySandboxNotice);

namespace privacy_sandbox {

// Manages all interfacing with UserEducation code.
class PrivacySandboxQueueManager {
 public:
  explicit PrivacySandboxQueueManager(Profile* profile);
  PrivacySandboxQueueManager();  // Default constructor.
  virtual ~PrivacySandboxQueueManager();

  // Triggered by product messaging code when turn in queue has arrived.
  // Moves the handle to temporary location to hold it.
  void HoldQueueHandle(
      user_education::RequiredNoticePriorityHandle messaging_priority_handle);
  // If the notice is in the queue, it will unqueue it. Otherwise, if the handle
  // is being held, it will release the handle.
  // Marked virtual for tests.
  virtual void MaybeUnqueueNotice();
  // If a prompt is required and we are not already in the queue or holding the
  // handle, will add the notice to the product messaging controller queue.
  // Marked virtual for tests.
  virtual void MaybeQueueNotice();
  // Tracks whether the queue is meant to be suppressed or not. If set to true,
  // queueing and unqueueing attempts will be ignored, but the existing queue
  // will be unaffected.
  void SetSuppressQueue(bool suppress_queue);
  // Sets the shown flag on the product messaging side.
  void SetQueueHandleShown();
  // Returns true if the Privacy Sandbox notice is in queue. Exposed for testing
  // purposes.
  bool IsNoticeQueued();
  // Returns instance of product messaging controller. Exposed for testing
  // purposes.
  user_education::ProductMessagingController* GetProductMessagingController();
  // Returns true if the handle is currently being held.
  bool IsHoldingHandle();
  // Emits queue state metrics when IsHoldingHandle check fails in
  // DidFinishNavigation.
  void MaybeEmitQueueStateMetrics();

 private:
  raw_ptr<user_education::ProductMessagingController>
      product_messaging_controller_;
  raw_ptr<Profile> profile_;
  // Temporary flag signifying not to requeue if the prompt has been suppressed.
  // TODO(crbug.com/370804492): When we add DMA notice to queue, remove this.
  bool suppress_queue_ = false;
  user_education::RequiredNoticePriorityHandle notice_handle_;
  int handle_check_failed_count_ = 0;
  base::WeakPtrFactory<PrivacySandboxQueueManager> weak_factory_{this};
};
}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_QUEUE_MANAGER_H_
