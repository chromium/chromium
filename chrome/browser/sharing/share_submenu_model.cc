// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/share_submenu_model.h"

#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"

namespace sharing {

namespace {

bool ShouldUseSendTabToSelfIcons() {
#if defined(OS_MAC)
  return false;
#else
  return true;
#endif
}

}  // namespace

ShareSubmenuModel::ShareSubmenuModel(Browser* browser,
                                     Context context,
                                     GURL url)
    : ui::SimpleMenuModel(this),
      browser_(browser),
      context_(context),
      url_(url) {
  AddGenerateQRCodeItem();
  AddSendTabToSelfItem();
}

ShareSubmenuModel::~ShareSubmenuModel() = default;

void ShareSubmenuModel::ExecuteCommand(int id, int event_flags) {
  switch (id) {
    case IDC_CONTENT_CONTEXT_GENERATE_QR_CODE:
      GenerateQRCode();
      break;
    case IDC_SEND_TAB_TO_SELF_SINGLE_TARGET:
      SendTabToSelfSingleTarget();
      break;
  }
}

void ShareSubmenuModel::AddGenerateQRCodeItem() {
  switch (context_) {
    case Context::IMAGE:
      AddItemWithStringId(IDC_CONTENT_CONTEXT_GENERATE_QR_CODE,
                          IDS_CONTEXT_MENU_GENERATE_QR_CODE_IMAGE);
      break;
    case Context::PAGE:
      AddItemWithStringId(IDC_CONTENT_CONTEXT_GENERATE_QR_CODE,
                          IDS_CONTEXT_MENU_GENERATE_QR_CODE_PAGE);
      break;
    case Context::LINK:
      NOTIMPLEMENTED();
      break;
    default:
      break;
  }
}

void ShareSubmenuModel::AddSendTabToSelfItem() {
  size_t devices = send_tab_to_self::GetValidDeviceCount(browser_->profile());

  if (devices == 0)
    return;

  if (devices == 1) {
    AddSendTabToSelfSingleTargetItem();
    return;
  }

  int label_id, command_id;
  send_tab_to_self::SendTabToSelfMenuType menu_type;
  if (context_ == Context::LINK) {
    label_id = IDS_LINK_MENU_SEND_TAB_TO_SELF;
    command_id = IDC_CONTENT_LINK_SEND_TAB_TO_SELF;
    menu_type = send_tab_to_self::SendTabToSelfMenuType::kLink;
  } else {
    label_id = IDS_CONTEXT_MENU_SEND_TAB_TO_SELF;
    command_id = IDC_SEND_TAB_TO_SELF;
    menu_type = send_tab_to_self::SendTabToSelfMenuType::kContent;
  }

  stts_submenu_model_ =
      std::make_unique<send_tab_to_self::SendTabToSelfSubMenuModel>(
          browser_->tab_strip_model()->GetActiveWebContents(), menu_type);
  if (ShouldUseSendTabToSelfIcons()) {
    AddSubMenuWithStringIdAndIcon(
        command_id, label_id, stts_submenu_model_.get(),
        ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
  } else {
    AddSubMenuWithStringId(command_id, label_id, stts_submenu_model_.get());
  }
}

void ShareSubmenuModel::AddSendTabToSelfSingleTargetItem() {
  std::u16string label = l10n_util::GetStringFUTF16(
      IDS_LINK_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
      send_tab_to_self::GetSingleTargetDeviceName(browser_->profile()));
  int command_id = context_ == Context::LINK
                       ? IDC_CONTENT_LINK_SEND_TAB_TO_SELF_SINGLE_TARGET
                       : IDC_SEND_TAB_TO_SELF_SINGLE_TARGET;
  if (ShouldUseSendTabToSelfIcons()) {
    AddItemWithIcon(command_id, label,
                    ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
  } else {
    AddItem(command_id, label);
  }
}

void ShareSubmenuModel::GenerateQRCode() {
  auto* web_contents = browser_->tab_strip_model()->GetActiveWebContents();
  auto* bubble_controller =
      qrcode_generator::QRCodeGeneratorBubbleController::Get(web_contents);

  if (context_ == Context::IMAGE) {
    base::RecordAction(base::UserMetricsAction(
        "SharingQRCode.DialogLaunched.ContextMenuImage"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "SharingQRCode.DialogLaunched.ContextMenuPage"));
  }

  bubble_controller->ShowBubble(url_);
}

void ShareSubmenuModel::SendTabToSelfSingleTarget() {
  if (context_ == Context::LINK) {
    send_tab_to_self::ShareToSingleTarget(
        browser_->tab_strip_model()->GetActiveWebContents(), url_);
    send_tab_to_self::RecordDeviceClicked(
        send_tab_to_self::ShareEntryPoint::kLinkMenu);
  } else {
    send_tab_to_self::ShareToSingleTarget(
        browser_->tab_strip_model()->GetActiveWebContents());
    send_tab_to_self::RecordDeviceClicked(
        send_tab_to_self::ShareEntryPoint::kContentMenu);
  }
}

}  // namespace sharing
