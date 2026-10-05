// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/devtools/global_confirm_info_bar.h"
#include "chrome/browser/extensions/api/debugger/debugger_api.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/strings/grit/ui_strings.h"

// Android uses messages instead of infobars.
static_assert(!BUILDFLAG(IS_ANDROID));

namespace extensions {

namespace {

using Delegates = std::map<ExtensionId, ExtensionDevToolsInfoBarDelegate*>;
Delegates& GetDelegates() {
  static base::NoDestructor<Delegates> delegates;
  return *delegates;
}

}  // namespace

// static
constexpr base::TimeDelta ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay;

base::CallbackListSubscription ExtensionDevToolsInfoBarDelegate::Create(
    const ExtensionId& extension_id,
    const std::string& extension_name,
    base::OnceClosure destroyed_callback) {
  Delegates& delegates = GetDelegates();
  const auto it = delegates.find(extension_id);
  if (it != delegates.end()) {
    it->second->timer_.Stop();
    return it->second->RegisterDestroyedCallback(std::move(destroyed_callback));
  }

  // Can't use std::make_unique<>(), constructor is private.
  auto delegate = base::WrapUnique(
      new ExtensionDevToolsInfoBarDelegate(extension_id, extension_name));
  auto* delegate_raw = delegate.get();
  delegates[extension_id] = delegate_raw;
  base::CallbackListSubscription subscription =
      delegate->RegisterDestroyedCallback(std::move(destroyed_callback));
  delegate_raw->infobar_ = GlobalConfirmInfoBar::Show(std::move(delegate));
  return subscription;
}

ExtensionDevToolsInfoBarDelegate::~ExtensionDevToolsInfoBarDelegate() {
  callback_list_.Notify();
  const size_t erased = GetDelegates().erase(extension_id_);
  CHECK(erased, base::NotFatalUntil::M161);
}

infobars::InfoBarDelegate::InfoBarIdentifier
ExtensionDevToolsInfoBarDelegate::GetIdentifier() const {
  return EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE;
}

bool ExtensionDevToolsInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

std::u16string ExtensionDevToolsInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_DEV_TOOLS_INFOBAR_LABEL,
      GetExtensionNameForDevToolsWarning(extension_name_));
}

gfx::ElideBehavior ExtensionDevToolsInfoBarDelegate::GetMessageElideBehavior()
    const {
  // The important part of the message text above is at the end:
  // "... is debugging the browser". If the extension name is very long,
  // we'd rather truncate it instead. See https://crbug.com/40090846.
  // See also the comment for kMaxExtensionNameLength in debugger_api.cc.
  return gfx::ELIDE_HEAD;
}

int ExtensionDevToolsInfoBarDelegate::GetButtons() const {
  // Android does not allow infobars with a solitary "Cancel" button, because
  // "Cancel" is considered a "secondary" button and cannot exist without a
  // primary button. Since the primary action here is to cancel, use BUTTON_OK
  // but label it as "Cancel" below and map Accept() to Cancel() below. This
  // works across platforms and avoids assertion failures deep in the Android
  // infobar code.
  return BUTTON_OK;
}

std::u16string ExtensionDevToolsInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}

bool ExtensionDevToolsInfoBarDelegate::Accept() {
  // See comment in GetButtons() above.
  return Cancel();
}

ExtensionDevToolsInfoBarDelegate::ExtensionDevToolsInfoBarDelegate(
    ExtensionId extension_id,
    const std::string& extension_name)
    : extension_id_(std::move(extension_id)),
      extension_name_(base::UTF8ToUTF16(extension_name)) {
  callback_list_.set_removal_callback(base::BindRepeating(
      &ExtensionDevToolsInfoBarDelegate::MaybeStartAutocloseTimer,
      base::Unretained(this)));
}

base::CallbackListSubscription
ExtensionDevToolsInfoBarDelegate::RegisterDestroyedCallback(
    base::OnceClosure destroyed_callback) {
  return callback_list_.Add(std::move(destroyed_callback));
}

void ExtensionDevToolsInfoBarDelegate::MaybeStartAutocloseTimer() {
  if (callback_list_.empty()) {
    // infobar_ was set in Create() which makes the following access safe.
    timer_.Start(FROM_HERE, kAutoCloseDelay, infobar_.get(),
                 &GlobalConfirmInfoBar::Close);
  }
}

}  // namespace extensions
