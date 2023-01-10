// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/near_oom_reduction_message_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace oom_intervention {

NearOomReductionMessageDelegate::NearOomReductionMessageDelegate() = default;

NearOomReductionMessageDelegate::~NearOomReductionMessageDelegate() {
  DismissMessage(messages::DismissReason::UNKNOWN);
}

void NearOomReductionMessageDelegate::ShowMessage(
    content::WebContents* web_contents,
    InterventionDelegate* intervention_delegate) {
  if (message_) {
    // There is already an active near OOM reduction message.
    return;
  }

  intervention_delegate_ = std::move(intervention_delegate);

  // Unretained is safe because this will always outlive message_ which owns
  // the callback.
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::NEAR_OOM_REDUCTION,
      base::BindOnce(
          &NearOomReductionMessageDelegate::HandleDeclineInterventionClicked,
          base::Unretained(this)),
      base::BindOnce(&NearOomReductionMessageDelegate::HandleMessageDismissed,
                     base::Unretained(this)));

  message_->SetTitle(
      l10n_util::GetStringUTF16(IDS_NEAR_OOM_REDUCTION_MESSAGE_TITLE));
  message_->SetDescription(
      l10n_util::GetStringUTF16(IDS_NEAR_OOM_REDUCTION_MESSAGE_DESCRIPTION));
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_SHOW_CONTENT));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_IC_MOBILE_FRIENDLY));

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}

void NearOomReductionMessageDelegate::DismissMessage(
    messages::DismissReason dismiss_reason) {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(message_.get(),
                                                             dismiss_reason);
  }
}

void NearOomReductionMessageDelegate::HandleDeclineInterventionClicked() {
  intervention_delegate_->DeclineInterventionWithReload();
}

void NearOomReductionMessageDelegate::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  DCHECK(message_);
  message_.reset();
}

}  // namespace oom_intervention
