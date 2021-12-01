// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_submenu_model.h"

#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/share/share_metrics.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/sharing_hub/sharing_hub_service.h"
#include "chrome/browser/sharing_hub/sharing_hub_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"

namespace share {

namespace {

// Third party command IDs are allocated by SharingHubModel starting at 0, but that conflicts
// with the dynamic IDs allocated for text editing commands by views::Textfield, so offset those
// command IDs here. See https://crbug.com/1259293 for more details.
const int kMenuCommandIdOffset = 2000;

// TODO(ellyjones): This is duplicated from RenderViewContextMenu, where it
// doesn't really belong. There is a note on the RenderViewContextMenu to remove
// it once it is no longer needed there, after https://crbug.com/1250494 is
// fixed.
std::u16string FormatURLForClipboard(const GURL& url) {
  DCHECK(!url.is_empty());
  DCHECK(url.is_valid());

  GURL url_to_format = url;
  url_formatter::FormatUrlTypes format_types;
  net::UnescapeRule::Type unescape_rules;
  if (url.SchemeIs(url::kMailToScheme)) {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    url_to_format = url.ReplaceComponents(replacements);
    format_types = url_formatter::kFormatUrlOmitMailToScheme;
    unescape_rules =
        net::UnescapeRule::PATH_SEPARATORS |
        net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS;
  } else {
    format_types = url_formatter::kFormatUrlOmitNothing;
    unescape_rules = net::UnescapeRule::NONE;
  }

  return url_formatter::FormatUrl(url_to_format, format_types, unescape_rules,
                                  nullptr, nullptr, nullptr);
}

}  // namespace

const base::Feature kShareMenu{
    "ShareMenu",
    base::FEATURE_DISABLED_BY_DEFAULT,
};

// static
bool ShareSubmenuModel::IsEnabled() {
  return base::FeatureList::IsEnabled(kShareMenu) ||
         share::AreUpcomingSharingFeaturesEnabled();
}

ShareSubmenuModel::ShareSubmenuModel(
    content::WebContents* web_contents,
    std::unique_ptr<ui::DataTransferEndpoint> source_endpoint,
    Context context,
    GURL url,
    std::u16string text)
    : ui::SimpleMenuModel(this),
      web_contents_(web_contents),
      source_endpoint_(std::move(source_endpoint)),
      context_(context),
      url_(url),
      text_(text) {
  // These methods will silently not add the specified item if it doesn't apply
  // to the given context or URL.
  AddGenerateQRCodeItem();
  AddSendTabToSelfItem();
  AddCopyLinkItem();
  // Temporarily disabled: https://crbug.com/1272875
  // AddShareToThirdPartyItems();
}

ShareSubmenuModel::~ShareSubmenuModel() = default;

void ShareSubmenuModel::ExecuteCommand(int id, int event_flags) {
  any_option_selected_for_metrics_ = true;
  LogShareSourceDesktop(ShareSourceDesktop::kWebContextMenu);
  switch (id) {
    case IDC_CONTENT_CONTEXT_GENERATE_QR_CODE:
      base::RecordAction(
          base::UserMetricsAction("ShareSubmenu.QRCodeSelected"));
      GenerateQRCode();
      break;
    case IDC_SEND_TAB_TO_SELF:
      base::RecordAction(
          base::UserMetricsAction("ShareSubmenu.SendTabToSelfSelected"));
      SendTabToSelf();
      break;
    case IDC_CONTENT_CONTEXT_COPYLINKLOCATION:
    case IDC_CONTENT_CONTEXT_COPYIMAGELOCATION:
      base::RecordAction(
          base::UserMetricsAction("ShareSubmenu.CopyLinkSelected"));
      CopyLink();
      break;
    default:
      base::RecordAction(
          base::UserMetricsAction("ShareSubmenu.ThirdPartySelected"));
      DCHECK_GE(id, kMenuCommandIdOffset);
      ShareToThirdParty(id - kMenuCommandIdOffset);
      break;
  }
}

void ShareSubmenuModel::OnMenuWillShow(SimpleMenuModel* source) {
  menu_opened_for_metrics_ = true;
}

void ShareSubmenuModel::MenuClosed(SimpleMenuModel* source) {
  if (menu_opened_for_metrics_ && !any_option_selected_for_metrics_)
      base::RecordAction(base::UserMetricsAction("ShareSubmenu.Abandoned"));

  // Reset the opened flag - it's possible for the same MenuModel to be opened &
  // closed multiple times and we want to log each separate abandon or choice.
  menu_opened_for_metrics_ = false;
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
      AddItemWithStringId(IDC_CONTENT_CONTEXT_GENERATE_QR_CODE,
                          IDS_CONTEXT_MENU_GENERATE_QR_CODE_LINK);
      break;
    default:
      break;
  }
}

void ShareSubmenuModel::AddSendTabToSelfItem() {
  // Allowed in tests.
  if (!web_contents_)
    return;

  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile());
  if (!send_tab_to_self::ShouldOfferToShareUrl(service, url_))
    return;

  // Only offer STTS when the context is actually the entire page; STTS can't
  // currently be used on links or images.
  if (context_ == Context::PAGE) {
    AddItemWithStringId(IDC_SEND_TAB_TO_SELF,
                        IDS_CONTEXT_MENU_SEND_TAB_TO_SELF);
  }
}

void ShareSubmenuModel::AddCopyLinkItem() {
  if (context_ == Context::LINK && url_.is_valid()) {
    AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYLINKLOCATION,
                        url_.SchemeIs(url::kMailToScheme)
                            ? IDS_CONTENT_CONTEXT_COPYEMAILADDRESS
                            : IDS_CONTENT_CONTEXT_COPYLINKLOCATION);
  } else if (context_ == Context::IMAGE) {
    AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGELOCATION,
                        IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);
  }
}

void ShareSubmenuModel::AddShareToThirdPartyItems() {
  auto* model = GetSharingHubModel();
  if (!model)
    return;

  // TODO(https://crbug.com/1252160): Support 3P items for link and image
  // targets.
  if (context_ == Context::IMAGE || context_ == Context::LINK)
    return;

  AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
  AddItemWithStringId(0, IDS_SHARING_HUB_SHARE_LABEL);
  SetEnabledAt(GetItemCount() - 1, false);

  std::vector<sharing_hub::SharingHubAction> actions;
  model->GetThirdPartyActionList(&actions);

  for (const auto& action : actions) {
    auto image = ui::ImageModel::FromImageSkia(action.third_party_icon);
    AddItemWithIcon(action.command_id + kMenuCommandIdOffset, action.title,
                    image);
    SetAccessibleNameAt(
        GetItemCount() - 1,
        l10n_util::GetStringFUTF16(IDS_SHARING_HUB_SHARE_LABEL_ACCESSIBILITY,
                                   action.title));
  }
}

void ShareSubmenuModel::GenerateQRCode() {
  auto* bubble_controller =
      qrcode_generator::QRCodeGeneratorBubbleController::Get(web_contents_);

  if (context_ == Context::IMAGE) {
    base::RecordAction(base::UserMetricsAction(
        "SharingQRCode.DialogLaunched.ContextMenuImage"));
  } else if (context_ == Context::LINK) {
    base::RecordAction(base::UserMetricsAction(
        "SharingQRCode.DialogLaunched.ContextMenuLink"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "SharingQRCode.DialogLaunched.ContextMenuPage"));
  }

  bubble_controller->ShowBubble(url_);
}

void ShareSubmenuModel::SendTabToSelf() {
  send_tab_to_self::SendTabToSelfBubbleController* controller =
      send_tab_to_self::SendTabToSelfBubbleController::
          CreateOrGetFromWebContents(web_contents_);
  controller->ShowBubble();
}

void ShareSubmenuModel::CopyLink() {
  if (url_.is_empty() || !url_.is_valid())
    return;

  auto source_endpoint_copy =
      source_endpoint_
          ? std::make_unique<ui::DataTransferEndpoint>(*source_endpoint_)
          : nullptr;
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste,
                                std::move(source_endpoint_copy));
  scw.WriteText(FormatURLForClipboard(url_));
}

void ShareSubmenuModel::ShareToThirdParty(int id) {
  auto* model = GetSharingHubModel();
  DCHECK(model);

  model->ExecuteThirdPartyAction(GetProfile(), url_, text_, id);
}

sharing_hub::SharingHubModel* ShareSubmenuModel::GetSharingHubModel() {
  // Allowed in unit tests.
  if (!web_contents_)
    return nullptr;

  sharing_hub::SharingHubService* const service =
      sharing_hub::SharingHubServiceFactory::GetForProfile(GetProfile());
  return service ? service->GetSharingHubModel() : nullptr;
}

Profile* ShareSubmenuModel::GetProfile() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace share
