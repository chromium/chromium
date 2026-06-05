// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/debugger/extension_dev_tools_message_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

// Android uses messages; Win/Mac/Linux/Chrome OS use infobars.
static_assert(BUILDFLAG(IS_ANDROID));

namespace extensions {

ExtensionDevToolsMessageDelegate::ExtensionDevToolsMessageDelegate(
    const std::string& extension_name,
    base::OnceClosure dismissed_callback)
    : dismissed_callback_(std::move(dismissed_callback)) {
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::EXTENSION_DEV_TOOLS,
      base::BindOnce(&ExtensionDevToolsMessageDelegate::OnActionClick,
                     base::Unretained(this)),
      base::BindOnce(&ExtensionDevToolsMessageDelegate::OnMessageDismissed,
                     base::Unretained(this)));

  const size_t kMaxExtensionNameLength = 1000;
  std::u16string ext_name = base::UTF8ToUTF16(extension_name);
  // TODO(crbug.com/518840663): Rename the IDS and IDR resource names to remove
  // "infobar". Perhaps replace with "warning"?
  message_->SetTitle(
      l10n_util::GetStringFUTF16(IDS_DEV_TOOLS_INFOBAR_LABEL,
                                 ext_name.substr(0, kMaxExtensionNameLength)));
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_APP_CANCEL));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_WARNING));
  // Unlike infobars, messages don't have an option for "never expire".
  // Use an arbitrary but long length of time.
  message_->SetDuration(base::Days(365).InMilliseconds());
}

ExtensionDevToolsMessageDelegate::~ExtensionDevToolsMessageDelegate() {
  if (message_ && message_->is_in_queue()) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::DISMISSED_BY_FEATURE);
  }
}

void ExtensionDevToolsMessageDelegate::Show(ui::WindowAndroid* window) {
  CHECK(window);
  // Priority urgent because this is a privacy/security concern. The extension
  // could read the user's data.
  messages::MessageDispatcherBridge::Get()->EnqueueWindowScopedMessage(
      message_.get(), window, messages::MessagePriority::kUrgent);
}

void ExtensionDevToolsMessageDelegate::OnActionClick() {
  // The primary action is a cancel button, so dismiss the message.
  OnMessageDismissed(messages::DismissReason::PRIMARY_ACTION);
}

void ExtensionDevToolsMessageDelegate::OnMessageDismissed(
    messages::DismissReason reason) {
  if (reason == messages::DismissReason::DISMISSED_BY_FEATURE) {
    // We're being deleted, don't call into our owner.
    return;
  }
  if (dismissed_callback_) {
    std::move(dismissed_callback_).Run();
  }
}

}  // namespace extensions
