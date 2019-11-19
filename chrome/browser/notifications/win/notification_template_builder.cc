// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/notification_template_builder.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/win/notification_image_retainer.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "third_party/libxml/chromium/xml_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// The different types of text nodes to output.
enum class TextType { NORMAL, ATTRIBUTION };

// Label to override context menu items in tests.
const char* context_menu_label_override = nullptr;

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
const char kDuration[] = "duration";
const char kDurationLong[] = "long";
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

// Formats the |origin| for display in the notification template.
std::string FormatOrigin(const GURL& origin) {
  base::string16 origin_string = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(origin),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  DCHECK(origin_string.size());

  return base::UTF16ToUTF8(origin_string);
}

// Writes the <toast> element with a given |launch_attribute|.
// Also closes the |xml_writer_| for writing as the toast is now complete.
void StartToastElement(XmlWriter* xml_writer,
                       const NotificationLaunchId& launch_id,
                       const message_center::Notification& notification) {
  xml_writer->StartElement(kNotificationToastElement);
  xml_writer->AddAttribute(kNotificationLaunchAttribute, launch_id.Serialize());

  if (notification.never_timeout()) {
    if (base::FeatureList::IsEnabled(
            features::kNotificationDurationLongForRequireInteraction)) {
      xml_writer->AddAttribute(kDuration, kDurationLong);
    } else {
      // Note: If the notification doesn't include a button, then Windows will
      // ignore the Reminder flag. See EnsureReminderHasButton below.
      xml_writer->AddAttribute(kScenario, kReminder);
    }
  }

  if (notification.timestamp().is_null())
    return;

  base::Time::Exploded exploded;
  notification.timestamp().UTCExplode(&exploded);
  xml_writer->AddAttribute(
      kToastElementDisplayTimestamp,
      base::StringPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ", exploded.year,
                         exploded.month, exploded.day_of_month, exploded.hour,
                         exploded.minute, exploded.second));
}

void EndToastElement(XmlWriter* xml_writer) {
  xml_writer->EndElement();
}

// Writes the <visual> element.
void StartVisualElement(XmlWriter* xml_writer) {
  xml_writer->StartElement(kVisualElement);
}

void EndVisualElement(XmlWriter* xml_writer) {
  xml_writer->EndElement();
}

// Writes the <binding> element with the given |template_name|.
void StartBindingElement(XmlWriter* xml_writer,
                         const std::string& template_name) {
  xml_writer->StartElement(kBindingElement);
  xml_writer->AddAttribute(kBindingElementTemplateAttribute, template_name);
}

void EndBindingElement(XmlWriter* xml_writer) {
  xml_writer->EndElement();
}

// Writes the <text> element with the given |content|. If |text_type| is
// ATTRIBUTION then |content| is treated as the source that the notification is
// attributed to.
void WriteTextElement(XmlWriter* xml_writer,
                      const std::string& content,
                      TextType text_type) {
  xml_writer->StartElement(kTextElement);
  if (text_type == TextType::ATTRIBUTION)
    xml_writer->AddAttribute(kPlacement, kAttribution);
  xml_writer->AppendElementContent(content);
  xml_writer->EndElement();
}

// Writes the <text> element containing the list entries.
void WriteItems(XmlWriter* xml_writer,
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
  WriteTextElement(xml_writer, item_list, TextType::NORMAL);
}

// A helper for constructing image xml.
void WriteImageElement(XmlWriter* xml_writer,
                       NotificationImageRetainer* image_retainer,
                       const gfx::Image& image,
                       const std::string& placement,
                       const std::string& hint_crop) {
  base::FilePath path = image_retainer->RegisterTemporaryImage(image);
  if (!path.empty()) {
    xml_writer->StartElement(kImageElement);
    xml_writer->AddAttribute(kPlacement, placement);
    xml_writer->AddAttribute(kSrc, base::UTF16ToUTF8(path.value()));
    if (!hint_crop.empty())
      xml_writer->AddAttribute(kHintCrop, hint_crop);
    xml_writer->EndElement();
  }
}

// Writes the <image> element for the notification icon.
void WriteIconElement(XmlWriter* xml_writer,
                      NotificationImageRetainer* image_retainer,
                      const message_center::Notification& notification) {
  WriteImageElement(xml_writer, image_retainer, notification.icon(),
                    kPlacementAppLogoOverride, kHintCropNone);
}

// Writes the <image> element for showing a large image within the notification
// body.
void WriteLargeImageElement(XmlWriter* xml_writer,
                            NotificationImageRetainer* image_retainer,
                            const message_center::Notification& notification) {
  WriteImageElement(xml_writer, image_retainer, notification.image(), kHero,
                    std::string());
}

// Adds a progress bar to the notification XML.
void WriteProgressElement(XmlWriter* xml_writer,
                          const message_center::Notification& notification) {
  // Two other attributes are supported by Microsoft:
  // title: A string shown on the left side of the toast, just above the bar.
  // valueStringOverride: A string that replaces the percentage on the right.
  xml_writer->StartElement(kProgress);
  // Status is mandatory, without it the progress bar is not shown.
  xml_writer->AddAttribute(kStatus, std::string());
  xml_writer->AddAttribute(
      kValue, base::StringPrintf("%3.2f", 1.0 * notification.progress() / 100));
  xml_writer->EndElement();
}

// Writes the <actions> element.
void StartActionsElement(XmlWriter* xml_writer) {
  xml_writer->StartElement(kActionsElement);
}

void EndActionsElement(XmlWriter* xml_writer) {
  xml_writer->EndElement();
}

// A helper for constructing action xml.
void WriteActionElement(XmlWriter* xml_writer,
                        NotificationImageRetainer* image_retainer,
                        const message_center::ButtonInfo& button,
                        int index,
                        NotificationLaunchId copied_launch_id) {
  xml_writer->StartElement(kActionElement);
  xml_writer->AddAttribute(kActivationType, kForeground);
  xml_writer->AddAttribute(kContent, base::UTF16ToUTF8(button.title));
  copied_launch_id.set_button_index(index);
  xml_writer->AddAttribute(kArguments, copied_launch_id.Serialize());

  if (!button.icon.IsEmpty()) {
    base::FilePath path = image_retainer->RegisterTemporaryImage(button.icon);
    if (!path.empty())
      xml_writer->AddAttribute(kImageUri, path.AsUTF8Unsafe());
  }

  xml_writer->EndElement();
}

// Fills in the details for the actions (the buttons the notification contains).
void AddActions(XmlWriter* xml_writer,
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
    xml_writer->StartElement(kInputElement);
    xml_writer->AddAttribute(kInputId, kUserResponse);
    xml_writer->AddAttribute(kInputType, kText);
    xml_writer->AddAttribute(kPlaceholderContent, placeholder);
    xml_writer->EndElement();
  }

  for (size_t i = 0; i < buttons.size(); ++i)
    WriteActionElement(xml_writer, image_retainer, buttons[i], i, launch_id);
}

// Writes the <audio silent="true"> element.
void WriteAudioSilentElement(XmlWriter* xml_writer) {
  xml_writer->StartElement(kAudioElement);
  xml_writer->AddAttribute(kSilent, kTrue);
  xml_writer->EndElement();
}

// A helper for constructing context menu xml.
void WriteContextMenuElement(XmlWriter* xml_writer,
                             const std::string& content,
                             const std::string& arguments) {
  xml_writer->StartElement(kActionElement);
  xml_writer->AddAttribute(kContent, content);
  xml_writer->AddAttribute(kPlacement, kContextMenu);
  xml_writer->AddAttribute(kActivationType, kForeground);
  xml_writer->AddAttribute(kArguments, arguments);
  xml_writer->EndElement();
}

// Adds context menu actions to the notification.
void AddContextMenu(XmlWriter* xml_writer,
                    NotificationLaunchId copied_launch_id,
                    const std::string& settings_msg) {
  copied_launch_id.set_is_for_context_menu();
  WriteContextMenuElement(xml_writer, settings_msg,
                          copied_launch_id.Serialize());
}

// Ensures that every reminder has at least one button, as the Action Center
// does not respect the Reminder setting on notifications with no buttons, so we
// must add a Dismiss button to the notification for those cases. For more
// details, see issue https://crbug.com/781792.
void EnsureReminderHasButton(XmlWriter* xml_writer,
                             const message_center::Notification& notification,
                             NotificationLaunchId copied_launch_id) {
  if (!notification.never_timeout() || !notification.buttons().empty() ||
      base::FeatureList::IsEnabled(
          features::kNotificationDurationLongForRequireInteraction)) {
    return;
  }

  xml_writer->StartElement(kActionElement);
  xml_writer->AddAttribute(kActivationType, kBackground);
  // TODO(finnur): Add our own string here (we're past string-freeze so we're
  // re-using the already translated "Close" from elsewhere).
  xml_writer->AddAttribute(
      kContent, l10n_util::GetStringUTF8(IDS_HISTORY_CLOSE_MENU_PROMO));
  copied_launch_id.set_is_for_dismiss_button();
  xml_writer->AddAttribute(kArguments, copied_launch_id.Serialize());
  xml_writer->EndElement();
}

}  // namespace

const char kNotificationToastElement[] = "toast";
const char kNotificationLaunchAttribute[] = "launch";

// libXml was preferred (over WinXml, which the samples in the link below tend
// to use) for building the XML template because it is used frequently in
// Chrome, is nicer to use and has already been vetted.
// https://docs.microsoft.com/en-us/windows/uwp/controls-and-patterns/tiles-and-notifications-adaptive-interactive-toasts
base::string16 BuildNotificationTemplate(
    NotificationImageRetainer* image_retainer,
    const NotificationLaunchId& launch_id,
    const message_center::Notification& notification) {
  DCHECK(image_retainer);

  XmlWriter xml_writer;
  xml_writer.StartWriting();

  StartToastElement(&xml_writer, launch_id, notification);
  StartVisualElement(&xml_writer);

  StartBindingElement(&xml_writer, kDefaultTemplate);

  // Content for the toast template.
  WriteTextElement(&xml_writer, base::UTF16ToUTF8(notification.title()),
                   TextType::NORMAL);

  // Message has historically not been shown for list-style notifications.
  if (notification.type() == message_center::NOTIFICATION_TYPE_MULTIPLE &&
      !notification.items().empty()) {
    WriteItems(&xml_writer, notification.items());
  } else {
    WriteTextElement(&xml_writer, base::UTF16ToUTF8(notification.message()),
                     TextType::NORMAL);
  }

  std::string attribution;
  if (notification.UseOriginAsContextMessage())
    attribution = FormatOrigin(notification.origin_url());
  else if (!notification.context_message().empty())
    attribution = base::UTF16ToUTF8(notification.context_message());

  if (!attribution.empty())
    WriteTextElement(&xml_writer, attribution, TextType::ATTRIBUTION);

  if (!notification.icon().IsEmpty())
    WriteIconElement(&xml_writer, image_retainer, notification);

  if (!notification.image().IsEmpty())
    WriteLargeImageElement(&xml_writer, image_retainer, notification);

  if (notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS)
    WriteProgressElement(&xml_writer, notification);

  EndBindingElement(&xml_writer);
  EndVisualElement(&xml_writer);

  StartActionsElement(&xml_writer);
  if (!notification.buttons().empty())
    AddActions(&xml_writer, image_retainer, notification, launch_id);
  EnsureReminderHasButton(&xml_writer, notification, launch_id);
  if (notification.should_show_settings_button()) {
    if (context_menu_label_override) {
      AddContextMenu(&xml_writer, launch_id, context_menu_label_override);
    } else {
      AddContextMenu(&xml_writer, launch_id,
                     l10n_util::GetStringUTF8(
                         IDS_WIN_NOTIFICATION_SETTINGS_CONTEXT_MENU_ITEM_NAME));
    }
  } else {
    DCHECK(!context_menu_label_override)
        << "Must show custom settings button label";
  }
  EndActionsElement(&xml_writer);

  if (notification.silent())
    WriteAudioSilentElement(&xml_writer);

  EndToastElement(&xml_writer);

  xml_writer.StopWriting();

  std::string template_xml = xml_writer.GetWrittenString();
  DCHECK(base::StartsWith(template_xml, kXmlVersionHeader,
                          base::CompareCase::SENSITIVE));

  // The |kXmlVersionHeader| is automatically appended by libxml, but the toast
  // system in the Windows Action Center expects it to be absent.
  return base::UTF8ToUTF16(
      base::StringPiece(template_xml).substr(sizeof(kXmlVersionHeader) - 1));
}

void SetContextMenuLabelForTesting(const char* label) {
  context_menu_label_override = label;
}
