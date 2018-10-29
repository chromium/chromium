// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/notification_template_builder.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/win/notification_image_retainer.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "third_party/libxml/chromium/libxml_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Constants used for the XML element names and their attributes.
const char kActionElement[] = "action";
const char kActionsElement[] = "actions";
const char kActivationType[] = "activationType";
const char kArguments[] = "arguments";
const char kAttribution[] = "attribution";
const char kAudioElement[] = "audio";
const char kBackground[] = "background";
const char kBindingElement[] = "binding";
const char kBindingElementTemplateAttribute[] = "template";
const char kContent[] = "content";
const char kContextMenu[] = "contextMenu";
const char kForeground[] = "foreground";
const char kHero[] = "hero";
const char kHintCrop[] = "hint-crop";
const char kHintCropNone[] = "none";
const char kImageElement[] = "image";
const char kImageUri[] = "imageUri";
const char kInputElement[] = "input";
const char kInputId[] = "id";
const char kInputType[] = "type";
const char kStatus[] = "status";
const char kPlaceholderContent[] = "placeHolderContent";
const char kPlacement[] = "placement";
const char kPlacementAppLogoOverride[] = "appLogoOverride";
const char kProgress[] = "progress";
const char kReminder[] = "reminder";
const char kScenario[] = "scenario";
const char kSilent[] = "silent";
const char kSrc[] = "src";
const char kText[] = "text";
const char kTextElement[] = "text";
const char kToastElementDisplayTimestamp[] = "displayTimestamp";
const char kTrue[] = "true";
const char kUserResponse[] = "userResponse";
const char kValue[] = "value";
const char kVisualElement[] = "visual";

// Name of the template used for default Chrome notifications.
const char kDefaultTemplate[] = "ToastGeneric";

// The XML version header that has to be stripped from the output.
const char kXmlVersionHeader[] = "<?xml version=\"1.0\"?>\n";

}  // namespace

const char kNotificationToastElement[] = "toast";
const char kNotificationLaunchAttribute[] = "launch";

// static
const char* NotificationTemplateBuilder::context_menu_label_override_ = nullptr;

// static
std::unique_ptr<NotificationTemplateBuilder> NotificationTemplateBuilder::Build(
    NotificationImageRetainer* image_retainer,
    const NotificationLaunchId& launch_id,
    const message_center::Notification& notification) {
  DCHECK(image_retainer);

  // Use base::WrapUnique + new because the constructor is private.
  auto builder = base::WrapUnique(new NotificationTemplateBuilder());

  builder->StartToastElement(launch_id, notification);
  builder->StartVisualElement();

  builder->StartBindingElement(kDefaultTemplate);

  // Content for the toast template.
  builder->WriteTextElement(base::UTF16ToUTF8(notification.title()),
                            TextType::NORMAL);

  // Message has historically not been shown for list-style notifications.
  if (notification.type() == message_center::NOTIFICATION_TYPE_MULTIPLE &&
      !notification.items().empty()) {
    builder->WriteItems(notification.items());
  } else {
    builder->WriteTextElement(base::UTF16ToUTF8(notification.message()),
                              TextType::NORMAL);
  }

  std::string attribution;
  if (notification.UseOriginAsContextMessage())
    attribution = builder->FormatOrigin(notification.origin_url());
  else if (!notification.context_message().empty())
    attribution = base::UTF16ToUTF8(notification.context_message());

  if (!attribution.empty())
    builder->WriteTextElement(attribution, TextType::ATTRIBUTION);

  if (!notification.icon().IsEmpty())
    builder->WriteIconElement(image_retainer, notification);

  if (!notification.image().IsEmpty())
    builder->WriteLargeImageElement(image_retainer, notification);

  if (notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS)
    builder->WriteProgressElement(notification);

  builder->EndBindingElement();
  builder->EndVisualElement();

  builder->StartActionsElement();
  if (!notification.buttons().empty())
    builder->AddActions(image_retainer, notification, launch_id);
  builder->EnsureReminderHasButton(notification, launch_id);
  builder->AddContextMenu(launch_id);
  builder->EndActionsElement();

  if (notification.silent())
    builder->WriteAudioSilentElement();

  builder->EndToastElement();

  return builder;
}

NotificationTemplateBuilder::NotificationTemplateBuilder()
    : xml_writer_(std::make_unique<XmlWriter>()) {
  xml_writer_->StartWriting();
}

NotificationTemplateBuilder::~NotificationTemplateBuilder() = default;

base::string16 NotificationTemplateBuilder::GetNotificationTemplate() const {
  std::string template_xml = xml_writer_->GetWrittenString();
  DCHECK(base::StartsWith(template_xml, kXmlVersionHeader,
                          base::CompareCase::SENSITIVE));

  // The |kXmlVersionHeader| is automatically appended by libxml, but the toast
  // system in the Windows Action Center expects it to be absent.
  return base::UTF8ToUTF16(template_xml.substr(sizeof(kXmlVersionHeader) - 1));
}

std::string NotificationTemplateBuilder::FormatOrigin(
    const GURL& origin) const {
  base::string16 origin_string = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(origin),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  DCHECK(origin_string.size());

  return base::UTF16ToUTF8(origin_string);
}

void NotificationTemplateBuilder::StartToastElement(
    const NotificationLaunchId& launch_id,
    const message_center::Notification& notification) {
  xml_writer_->StartElement(kNotificationToastElement);
  xml_writer_->AddAttribute(kNotificationLaunchAttribute,
                            launch_id.Serialize());

  // Note: If the notification doesn't include a button, then Windows will
  // ignore the Reminder flag.
  if (notification.never_timeout())
    xml_writer_->AddAttribute(kScenario, kReminder);

  if (notification.timestamp().is_null())
    return;

  base::Time::Exploded exploded;
  notification.timestamp().UTCExplode(&exploded);
  xml_writer_->AddAttribute(
      kToastElementDisplayTimestamp,
      base::StringPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ", exploded.year,
                         exploded.month, exploded.day_of_month, exploded.hour,
                         exploded.minute, exploded.second));
}

void NotificationTemplateBuilder::EndToastElement() {
  xml_writer_->EndElement();
  xml_writer_->StopWriting();
}

void NotificationTemplateBuilder::StartVisualElement() {
  xml_writer_->StartElement(kVisualElement);
}

void NotificationTemplateBuilder::EndVisualElement() {
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::StartBindingElement(
    const std::string& template_name) {
  xml_writer_->StartElement(kBindingElement);
  xml_writer_->AddAttribute(kBindingElementTemplateAttribute, template_name);
}

void NotificationTemplateBuilder::EndBindingElement() {
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::WriteTextElement(const std::string& content,
                                                   TextType text_type) {
  xml_writer_->StartElement(kTextElement);
  if (text_type == TextType::ATTRIBUTION)
    xml_writer_->AddAttribute(kPlacement, kAttribution);
  xml_writer_->AppendElementContent(content);
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::WriteItems(
    const std::vector<message_center::NotificationItem>& items) {
  // A toast can have a maximum of three text items, of which one is reserved
  // for the title. The remaining two can each handle up to four lines of text,
  // but the toast can only show four lines total, so there's no point in having
  // more than one text item. Therefore, we show them all in one and hope there
  // is no truncation at the bottom. There will never be room for items 5 and up
  // so we don't make an attempt to show them.
  constexpr size_t kMaxEntries = 4;
  size_t entries = std::min(kMaxEntries, items.size());
  std::string item_list;
  for (size_t i = 0; i < entries; ++i) {
    const auto& item = items[i];
    item_list += base::UTF16ToUTF8(item.title) + " - " +
                 base::UTF16ToUTF8(item.message) + "\n";
  }
  WriteTextElement(item_list, TextType::NORMAL);
}

void NotificationTemplateBuilder::WriteIconElement(
    NotificationImageRetainer* image_retainer,
    const message_center::Notification& notification) {
  WriteImageElement(image_retainer, notification.icon(),
                    kPlacementAppLogoOverride, kHintCropNone);
}

void NotificationTemplateBuilder::WriteLargeImageElement(
    NotificationImageRetainer* image_retainer,
    const message_center::Notification& notification) {
  WriteImageElement(image_retainer, notification.image(), kHero, std::string());
}

void NotificationTemplateBuilder::WriteImageElement(
    NotificationImageRetainer* image_retainer,
    const gfx::Image& image,
    const std::string& placement,
    const std::string& hint_crop) {
  base::FilePath path = image_retainer->RegisterTemporaryImage(image);
  if (!path.empty()) {
    xml_writer_->StartElement(kImageElement);
    xml_writer_->AddAttribute(kPlacement, placement);
    xml_writer_->AddAttribute(kSrc, base::UTF16ToUTF8(path.value()));
    if (!hint_crop.empty())
      xml_writer_->AddAttribute(kHintCrop, hint_crop);
    xml_writer_->EndElement();
  }
}

void NotificationTemplateBuilder::WriteProgressElement(
    const message_center::Notification& notification) {
  // Two other attributes are supported by Microsoft:
  // title: A string shown on the left side of the toast, just above the bar.
  // valueStringOverride: A string that replaces the percentage on the right.
  xml_writer_->StartElement(kProgress);
  // Status is mandatory, without it the progress bar is not shown.
  xml_writer_->AddAttribute(kStatus, std::string());
  xml_writer_->AddAttribute(
      kValue, base::StringPrintf("%3.2f", 1.0 * notification.progress() / 100));
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::AddActions(
    NotificationImageRetainer* image_retainer,
    const message_center::Notification& notification,
    const NotificationLaunchId& launch_id) {
  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  bool inline_reply = false;
  std::string placeholder;
  for (const auto& button : buttons) {
    if (!button.placeholder)
      continue;

    inline_reply = true;
    placeholder = base::UTF16ToUTF8(*button.placeholder);
    break;
  }

  if (inline_reply) {
    xml_writer_->StartElement(kInputElement);
    xml_writer_->AddAttribute(kInputId, kUserResponse);
    xml_writer_->AddAttribute(kInputType, kText);
    xml_writer_->AddAttribute(kPlaceholderContent, placeholder);
    xml_writer_->EndElement();
  }

  for (size_t i = 0; i < buttons.size(); ++i)
    WriteActionElement(image_retainer, buttons[i], i, launch_id);
}

void NotificationTemplateBuilder::AddContextMenu(
    NotificationLaunchId copied_launch_id) {
  std::string notification_settings_msg = l10n_util::GetStringUTF8(
      IDS_WIN_NOTIFICATION_SETTINGS_CONTEXT_MENU_ITEM_NAME);
  if (context_menu_label_override_)
    notification_settings_msg = context_menu_label_override_;

  copied_launch_id.set_is_for_context_menu();
  WriteContextMenuElement(notification_settings_msg,
                          copied_launch_id.Serialize());
}

void NotificationTemplateBuilder::StartActionsElement() {
  xml_writer_->StartElement(kActionsElement);
}

void NotificationTemplateBuilder::EndActionsElement() {
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::WriteAudioSilentElement() {
  xml_writer_->StartElement(kAudioElement);
  xml_writer_->AddAttribute(kSilent, kTrue);
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::WriteActionElement(
    NotificationImageRetainer* image_retainer,
    const message_center::ButtonInfo& button,
    int index,
    NotificationLaunchId copied_launch_id) {
  xml_writer_->StartElement(kActionElement);
  xml_writer_->AddAttribute(kActivationType, kForeground);
  xml_writer_->AddAttribute(kContent, base::UTF16ToUTF8(button.title));
  copied_launch_id.set_button_index(index);
  xml_writer_->AddAttribute(kArguments, copied_launch_id.Serialize());

  if (!button.icon.IsEmpty()) {
    base::FilePath path = image_retainer->RegisterTemporaryImage(button.icon);
    if (!path.empty())
      xml_writer_->AddAttribute(kImageUri, path.AsUTF8Unsafe());
  }

  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::WriteContextMenuElement(
    const std::string& content,
    const std::string& arguments) {
  xml_writer_->StartElement(kActionElement);
  xml_writer_->AddAttribute(kContent, content);
  xml_writer_->AddAttribute(kPlacement, kContextMenu);
  xml_writer_->AddAttribute(kActivationType, kForeground);
  xml_writer_->AddAttribute(kArguments, arguments);
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::EnsureReminderHasButton(
    const message_center::Notification& notification,
    NotificationLaunchId copied_launch_id) {
  if (!notification.never_timeout() || !notification.buttons().empty())
    return;

  xml_writer_->StartElement(kActionElement);
  xml_writer_->AddAttribute(kActivationType, kBackground);
  // TODO(finnur): Add our own string here (we're past string-freeze so we're
  // re-using the already translated "Close" from elsewhere).
  xml_writer_->AddAttribute(
      kContent, l10n_util::GetStringUTF8(IDS_MD_HISTORY_CLOSE_MENU_PROMO));
  copied_launch_id.set_is_for_dismiss_button();
  xml_writer_->AddAttribute(kArguments, copied_launch_id.Serialize());
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::OverrideContextMenuLabelForTesting(
    const char* label) {
  context_menu_label_override_ = label;
}
