// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/hung_plugin_infobar_delegate.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/ui/hung_plugin_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* HungPluginInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    HungPluginTabHelper* helper,
    int plugin_child_id,
    const std::u16string& plugin_name) {
  return infobar_manager->AddInfoBar(CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(new HungPluginInfoBarDelegate(
          helper, plugin_child_id, plugin_name))));
}

HungPluginInfoBarDelegate::HungPluginInfoBarDelegate(
    HungPluginTabHelper* helper,
    int plugin_child_id,
    const std::u16string& plugin_name)
    : ConfirmInfoBarDelegate(),
      helper_(helper),
      plugin_child_id_(plugin_child_id),
      message_(
          l10n_util::GetStringFUTF16(IDS_BROWSER_HANGMONITOR_PLUGIN_INFOBAR,
                                     plugin_name)),
      button_text_(l10n_util::GetStringUTF16(
          IDS_BROWSER_HANGMONITOR_PLUGIN_INFOBAR_KILLBUTTON)) {}

HungPluginInfoBarDelegate::~HungPluginInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
HungPluginInfoBarDelegate::GetIdentifier() const {
  return HUNG_PLUGIN_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& HungPluginInfoBarDelegate::GetVectorIcon() const {
  return kExtensionCrashedIcon;
}

std::u16string HungPluginInfoBarDelegate::GetMessageText() const {
  return message_;
}

int HungPluginInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string HungPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return button_text_;
}

bool HungPluginInfoBarDelegate::Accept() {
  helper_->KillPlugin(plugin_child_id_);
  return true;
}
