// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_toast.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace contextual_tasks {

// static
std::unique_ptr<ContextualTasksToast> ContextualTasksToast::Show(
    content::WebContents* web_contents,
    int title_res_id,
    int description_res_id) {
  if (!web_contents) {
    return nullptr;
  }
  return base::WrapUnique(
      new ContextualTasksToast(web_contents, title_res_id, description_res_id));
}

ContextualTasksToast::ContextualTasksToast(content::WebContents* web_contents,
                                           int title_res_id,
                                           int description_res_id)
    : web_contents_(web_contents->GetWeakPtr()) {
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::CONTEXTUAL_TASKS_WINDOW_RESIZED,
      base::BindOnce(&ContextualTasksToast::HandleMessageAccepted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ContextualTasksToast::HandleMessageDismissed,
                     weak_ptr_factory_.GetWeakPtr()));

  message_->SetTitle(l10n_util::GetStringUTF16(title_res_id));
  message_->SetDescription(l10n_util::GetStringUTF16(description_res_id));
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_OK));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_IC_GOOGLE_SERVICES));

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}

ContextualTasksToast::~ContextualTasksToast() {
  if (message_) {
    if (web_contents_) {
      messages::MessageDispatcherBridge::Get()->DismissMessage(
          message_.get(), messages::DismissReason::DISMISSED_BY_FEATURE);
    }
    message_.reset();
  }
}

void ContextualTasksToast::HandleMessageAccepted() {
  // No-op for now. Clicking OK just dismisses the toast.
}

void ContextualTasksToast::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  message_.reset();
}

}  // namespace contextual_tasks
