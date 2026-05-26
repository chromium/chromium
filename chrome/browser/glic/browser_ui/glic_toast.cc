// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_toast.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_wrapper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {

// static
std::unique_ptr<GlicToast> GlicToast::Show(content::WebContents* web_contents,
                                           int title_res_id,
                                           int description_res_id) {
  if (!web_contents) {
    return nullptr;
  }
  return std::make_unique<GlicToast>(web_contents, title_res_id,
                                     description_res_id);
}

GlicToast::GlicToast(content::WebContents* web_contents,
                     int title_res_id,
                     int description_res_id)
    : web_contents_(web_contents) {
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::GLIC_WINDOW_RESIZED,
      base::BindOnce(&GlicToast::HandleMessageAccepted, base::Unretained(this)),
      base::BindOnce(&GlicToast::HandleMessageDismissed,
                     base::Unretained(this)));

  message_->SetTitle(l10n_util::GetStringUTF16(title_res_id));
  message_->SetDescription(l10n_util::GetStringUTF16(description_res_id));
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_OK));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_IC_SPARK_BLUE));
  message_->DisableIconTint();

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);
}

GlicToast::~GlicToast() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::DISMISSED_BY_FEATURE);
  }
}

void GlicToast::HandleMessageAccepted() {
  // Action clicked. Default behavior dismisses.
}

void GlicToast::HandleMessageDismissed(messages::DismissReason reason) {
  message_.reset();
}

}  // namespace glic
