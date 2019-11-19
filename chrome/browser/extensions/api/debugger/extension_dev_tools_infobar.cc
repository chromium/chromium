// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/global_confirm_info_bar.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// The InfoBarDelegate that ExtensionDevToolsInfoBar shows.
class ExtensionDevToolsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ExtensionDevToolsInfoBarDelegate(const base::Closure& dismissed_callback,
                                   const std::string& client_name);
  ~ExtensionDevToolsInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  gfx::ElideBehavior GetMessageElideBehavior() const override;

  int GetButtons() const override;
  bool Cancel() override;

 private:
  const base::string16 client_name_;
  base::Closure dismissed_callback_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsInfoBarDelegate);
};

ExtensionDevToolsInfoBarDelegate::ExtensionDevToolsInfoBarDelegate(
    const base::Closure& dismissed_callback,
    const std::string& client_name)
    : ConfirmInfoBarDelegate(),
      client_name_(base::UTF8ToUTF16(client_name)),
      dismissed_callback_(dismissed_callback) {}

ExtensionDevToolsInfoBarDelegate::~ExtensionDevToolsInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
ExtensionDevToolsInfoBarDelegate::GetIdentifier() const {
  return EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE;
}

bool ExtensionDevToolsInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

void ExtensionDevToolsInfoBarDelegate::InfoBarDismissed() {
  DCHECK(!dismissed_callback_.is_null());
  std::move(dismissed_callback_).Run();
}

base::string16 ExtensionDevToolsInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_DEV_TOOLS_INFOBAR_LABEL, client_name_);
}

gfx::ElideBehavior ExtensionDevToolsInfoBarDelegate::GetMessageElideBehavior()
    const {
  // The important part of the message text above is at the end:
  // "... is debugging the browser". If the extension name is very long,
  // we'd rather truncate it instead. See https://crbug.com/823194.
  return gfx::ELIDE_HEAD;
}

int ExtensionDevToolsInfoBarDelegate::GetButtons() const {
  return BUTTON_CANCEL;
}

bool ExtensionDevToolsInfoBarDelegate::Cancel() {
  InfoBarDismissed();
  // InfoBarDismissed() will have closed us already.
  return false;
}

using ExtensionInfoBars = std::map<std::string, ExtensionDevToolsInfoBar*>;
base::LazyInstance<ExtensionInfoBars>::Leaky g_extension_info_bars =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ExtensionDevToolsInfoBar* ExtensionDevToolsInfoBar::Create(
    const std::string& extension_id,
    const std::string& extension_name,
    ExtensionDevToolsClientHost* client_host,
    const base::Closure& dismissed_callback) {
  auto it = g_extension_info_bars.Get().find(extension_id);
  ExtensionDevToolsInfoBar* infobar = nullptr;
  if (it != g_extension_info_bars.Get().end())
    infobar = it->second;
  else
    infobar = new ExtensionDevToolsInfoBar(extension_id, extension_name);
  infobar->callbacks_[client_host] = dismissed_callback;
  return infobar;
}

ExtensionDevToolsInfoBar::ExtensionDevToolsInfoBar(
    const std::string& extension_id,
    const std::string& extension_name)
    : extension_id_(extension_id) {
  g_extension_info_bars.Get()[extension_id] = this;

  // This class closes the |infobar_|, so it's safe to pass Unretained(this).
  auto delegate = std::make_unique<ExtensionDevToolsInfoBarDelegate>(
      base::Bind(&ExtensionDevToolsInfoBar::InfoBarDismissed,
                 base::Unretained(this)),
      extension_name);
  infobar_ = GlobalConfirmInfoBar::Show(std::move(delegate));
}

ExtensionDevToolsInfoBar::~ExtensionDevToolsInfoBar() {
  g_extension_info_bars.Get().erase(extension_id_);
  if (infobar_)
    infobar_->Close();
}

void ExtensionDevToolsInfoBar::Remove(
    ExtensionDevToolsClientHost* client_host) {
  callbacks_.erase(client_host);
  if (callbacks_.empty())
    delete this;
}

void ExtensionDevToolsInfoBar::InfoBarDismissed() {
  std::map<ExtensionDevToolsClientHost*, base::Closure> copy = callbacks_;
  for (const auto& pair : copy)
    pair.second.Run();
}

}  // namespace extensions
