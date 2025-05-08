// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_queue_manager.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt_helper.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/user_education/common/product_messaging_controller.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/privacy_sandbox/privacy_sandbox_queue_manager.h"
#endif  // !BUILDFLAG(IS_ANDROID)

DEFINE_REQUIRED_NOTICE_IDENTIFIER(kPrivacySandboxNotice);

namespace privacy_sandbox {

PrivacySandboxQueueManager::PrivacySandboxQueueManager(Profile* profile)
    : profile_(profile) {}
// Default constructor used for testing.
PrivacySandboxQueueManager::PrivacySandboxQueueManager() = default;
PrivacySandboxQueueManager::~PrivacySandboxQueueManager() = default;

user_education::ProductMessagingController*
PrivacySandboxQueueManager::GetProductMessagingController() {
  if (!product_messaging_controller_) {
    // If there is a valid service, set it.
    if (auto* service =
            UserEducationServiceFactory::GetForBrowserContext(profile_)) {
      product_messaging_controller_ = &service->product_messaging_controller();
    }
  }
  return product_messaging_controller_;
}

void PrivacySandboxQueueManager::SetSuppressQueue(bool suppress_queue) {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxNoticeQueue)) {
    return;
  }

  suppress_queue_ = suppress_queue;
}

void PrivacySandboxQueueManager::MaybeEmitQueueStateMetrics() {
  // We only want to count when we are not holding the handle.
  if (IsHoldingHandle()) {
    return;
  }

  handle_check_failed_count_++;
  if (IsNoticeQueued()) {
    base::UmaHistogramCounts100(
        "PrivacySandbox.Notice.NotHoldingHandle.InQueue",
        handle_check_failed_count_);
  } else {
    base::UmaHistogramCounts100(
        "PrivacySandbox.Notice.NotHoldingHandle.NotInQueue",
        handle_check_failed_count_);
  }
}

bool PrivacySandboxQueueManager::IsHoldingHandle() {
  return static_cast<bool>(notice_handle_);
}

void PrivacySandboxQueueManager::HoldQueueHandle(
    user_education::RequiredNoticePriorityHandle messaging_priority_handle) {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxNoticeQueue)) {
    return;
  }
  notice_handle_ = std::move(messaging_priority_handle);
}

void PrivacySandboxQueueManager::SetQueueHandleShown() {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxNoticeQueue)) {
    return;
  }
  notice_handle_.SetShown();
}

void PrivacySandboxQueueManager::MaybeUnqueueNotice() {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxNoticeQueue) ||
      suppress_queue_) {
    return;
  }

  // Release the handle if we are holding it (checked by controller).
  notice_handle_.Release();

  // Ensure we don't attempt to access the product messaging controller if it
  // doesn't exist.
  if (auto* product_messaging_controller = GetProductMessagingController()) {
    // Unqueue if we are in the queue (handled by controller).
    product_messaging_controller->UnqueueRequiredNotice(kPrivacySandboxNotice);
  }
}

bool PrivacySandboxQueueManager::IsNoticeQueued() {
  if (auto* product_messaging_controller = GetProductMessagingController()) {
    return product_messaging_controller->IsNoticeQueued(kPrivacySandboxNotice);
  }
  return false;
}

void PrivacySandboxQueueManager::MaybeQueueNotice() {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxNoticeQueue) ||
      suppress_queue_) {
    return;
  }

  // We don't want to queue in the case the profile does not require a prompt.
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile_);
  if (!privacy_sandbox_service ||
      privacy_sandbox_service->GetRequiredPromptType(
          PrivacySandboxService::SurfaceType::kDesktop) ==
          PrivacySandboxService::PromptType::kNone) {
    return;
  }

  // If we are already holding the handle or in the queue, we don't want to
  // requeue.
  if (IsHoldingHandle() || IsNoticeQueued()) {
    return;
  }

  if (auto* product_messaging_controller = GetProductMessagingController()) {
    product_messaging_controller->QueueRequiredNotice(
      kPrivacySandboxNotice,
      base::BindOnce(&PrivacySandboxQueueManager::HoldQueueHandle, weak_factory_.GetWeakPtr()), {/* TODO(crbug.com/370804492): When we add the DMA notice, add it to this show_after_ list*/});
  }
}
}  // namespace privacy_sandbox
