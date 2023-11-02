// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/reload_plugin_infobar_delegate.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/l10n/l10n_util.h"

// static
void ReloadPluginInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    content::NavigationController* controller,
    const std::u16string& message) {
  infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
          new ReloadPluginInfoBarDelegate(controller, message))));
}

ReloadPluginInfoBarDelegate::ReloadPluginInfoBarDelegate(
    content::NavigationController* controller,
    const std::u16string& message)
    : controller_(controller), message_(message) {}

ReloadPluginInfoBarDelegate::~ReloadPluginInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
ReloadPluginInfoBarDelegate::GetIdentifier() const {
  return RELOAD_PLUGIN_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& ReloadPluginInfoBarDelegate::GetVectorIcon() const {
  return kExtensionCrashedIcon;
}

std::u16string ReloadPluginInfoBarDelegate::GetMessageText() const {
  return message_;
}

int ReloadPluginInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string ReloadPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_RELOAD_PAGE_WITH_PLUGIN);
}

bool ReloadPluginInfoBarDelegate::Accept() {
  controller_->Reload(content::ReloadType::NORMAL, true);
  return true;
}
