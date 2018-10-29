// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_TEMPLATE_BUILDER_H_
#define CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_TEMPLATE_BUILDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/notifications/notification_common.h"

class GURL;
class NotificationLaunchId;
class XmlWriter;

namespace gfx {
class Image;
}

namespace message_center {
struct ButtonInfo;
class Notification;
struct NotificationItem;
}  // namespace message_center

class NotificationImageRetainer;

// The Notification Toast element name in the toast XML.
extern const char kNotificationToastElement[];

// The Notification Launch attribute name in the toast XML.
extern const char kNotificationLaunchAttribute[];

// Builds XML-based notification templates for displaying a given notification
// in the Windows Action Center.
//
// https://docs.microsoft.com/en-us/windows/uwp/controls-and-patterns/tiles-and-notifications-adaptive-interactive-toasts
//
// libXml was preferred (over WinXml, which the samples tend to use) because it
// is used frequently in Chrome, is nicer to use and has already been vetted.
class NotificationTemplateBuilder {
 public:
  // Builds the notification template for the given |notification|.
  static std::unique_ptr<NotificationTemplateBuilder> Build(
      NotificationImageRetainer* image_retainer,
      const NotificationLaunchId& launch_id,
      const message_center::Notification& notification);

  // Set label for the context menu item in testing. The caller owns |label| and
  // is responsible for resetting the override back to nullptr.
  static void OverrideContextMenuLabelForTesting(const char* label);

  ~NotificationTemplateBuilder();

  // Gets the XML template that was created by this builder.
  base::string16 GetNotificationTemplate() const;

 private:
  // The different types of text nodes to output.
  enum class TextType { NORMAL, ATTRIBUTION };

  NotificationTemplateBuilder();

  // Formats the |origin| for display in the notification template.
  std::string FormatOrigin(const GURL& origin) const;

  // Writes the <toast> element with a given |launch_attribute|.
  // Also closes the |xml_writer_| for writing as the toast is now complete.
  void StartToastElement(const NotificationLaunchId& launch_id,
                         const message_center::Notification& notification);
  void EndToastElement();

  // Writes the <visual> element.
  void StartVisualElement();
  void EndVisualElement();

  // Writes the <binding> element with the given |template_name|.
  void StartBindingElement(const std::string& template_name);
  void EndBindingElement();

  // Writes the <text> element with the given |content|. If |text_type| is
  // ATTRIBUTION then |content| is treated as the source that the notification
  // is attributed to.
  void WriteTextElement(const std::string& content, TextType text_type);

  // Writes the <text> element containing the list entries.
  void WriteItems(const std::vector<message_center::NotificationItem>& items);

  // Writes the <image> element for the notification icon.
  void WriteIconElement(NotificationImageRetainer* image_retainer,
                        const message_center::Notification& notification);

  // Writes the <image> element for showing a large image within the
  // notification body.
  void WriteLargeImageElement(NotificationImageRetainer* image_retainer,
                              const message_center::Notification& notification);

  // A helper for constructing image xml.
  void WriteImageElement(NotificationImageRetainer* image_retainer,
                         const gfx::Image& image,
                         const std::string& placement,
                         const std::string& hint_crop);

  // Adds a progress bar to the notification XML.
  void WriteProgressElement(const message_center::Notification& notification);

  // Writes the <actions> element.
  void StartActionsElement();
  void EndActionsElement();

  // Writes the <audio silent="true"> element.
  void WriteAudioSilentElement();

  // Fills in the details for the actions (the buttons the notification
  // contains).
  void AddActions(NotificationImageRetainer* image_retainer,
                  const message_center::Notification& notification,
                  const NotificationLaunchId& launch_id);
  void WriteActionElement(NotificationImageRetainer* image_retainer,
                          const message_center::ButtonInfo& button,
                          int index,
                          NotificationLaunchId copied_launch_id);

  // Adds context menu actions to the notification sent by |origin|.
  void AddContextMenu(NotificationLaunchId copied_launch_id);
  void WriteContextMenuElement(const std::string& content,
                               const std::string& arguments);

  // Ensures that every reminder has at least one button, as the Action Center
  // does not respect the Reminder setting on notifications with no buttons, so
  // we must add a Dismiss button to the notification for those cases. For more
  // details, see issue https://crbug.com/781792.
  void EnsureReminderHasButton(const message_center::Notification& notification,
                               NotificationLaunchId copied_launch_id);

  // Label to override context menu items in tests.
  static const char* context_menu_label_override_;

  // The XML writer to which the template will be written.
  std::unique_ptr<XmlWriter> xml_writer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationTemplateBuilder);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_TEMPLATE_BUILDER_H_
