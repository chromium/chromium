// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_toast.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {

namespace {

class GlicToastAndroid : public GlicToast {
 public:
  GlicToastAndroid(content::WebContents& web_contents,
                   int title_res_id,
                   int description_res_id) {
    message_ = std::make_unique<messages::MessageWrapper>(
        messages::MessageIdentifier::GLIC_WINDOW_RESIZED,
        base::BindOnce(&GlicToastAndroid::HandleMessageAccepted,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&GlicToastAndroid::HandleMessageDismissed,
                       weak_ptr_factory_.GetWeakPtr()));

    message_->SetTitle(l10n_util::GetStringUTF16(title_res_id));
    if (description_res_id != 0) {
      message_->SetDescription(l10n_util::GetStringUTF16(description_res_id));
    }
    message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_OK));
    message_->SetIconResourceId(
        ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_IC_SPARK_BLUE));
    message_->DisableIconTint();

    messages::MessageDispatcherBridge::Get()->EnqueueMessage(
        message_.get(), &web_contents, messages::MessageScopeType::WEB_CONTENTS,
        messages::MessagePriority::kNormal);
  }

  ~GlicToastAndroid() override {
    if (message_) {
      messages::MessageDispatcherBridge::Get()->DismissMessage(
          message_.get(), messages::DismissReason::DISMISSED_BY_FEATURE);
    }
  }

  bool IsShowing() const override { return message_ != nullptr; }

 private:
  void HandleMessageAccepted() {
    // Action clicked. Default behavior dismisses.
  }

  void HandleMessageDismissed(messages::DismissReason reason) {
    message_.reset();
  }

  std::unique_ptr<messages::MessageWrapper> message_;
  base::WeakPtrFactory<GlicToastAndroid> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<GlicToast> GlicToast::Show(const tabs::TabInterface& tab,
                                           int title_res_id,
                                           int description_res_id) {
  content::WebContents* web_contents = tab.GetContents();
  if (!web_contents) {
    return nullptr;
  }
  auto toast = std::make_unique<GlicToastAndroid>(*web_contents, title_res_id,
                                                  description_res_id);
  if (!toast->IsShowing()) {
    return nullptr;
  }
  return toast;
}

}  // namespace glic
