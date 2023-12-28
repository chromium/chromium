// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_NOTIFICATION_BUILDER_H_
#define ASH_PUBLIC_CPP_SYSTEM_NOTIFICATION_BUILDER_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

class GURL;

namespace gfx {
struct VectorIcon;
}

namespace ui::message_center {
class NotificationDelegate;
}  // namespace ui::message_center

namespace ash {

// Builder class for when the `CreateSystemNotification` factory method
// doesn't provide enough flexibility. This class can be useful when you're
// emitting multiple notifications in your component but they only differ in
// very few fields like Id, message and title or when some/many arguments are
// just default constructed. The first instinct may be to create a component
// specific factory method but that may not provide enough flexibility either,
// see the second example below.
//
// Example to reduce code duplication:
// void Foo::ShowNotification1() {
//   message_center::RichNotificationData optional_data;
//   optional_data.buttons.push_back(message_center::ButtonInfo(
//       l10n_util::GetStringUTF16(IDS_SOME_BUTTON)));
//
//    CreateSystemNotificationPtr(
//          message_center::NOTIFICATION_TYPE_SIMPLE,
//          kNotificationId1,
//          l10n_util::GetStringUTF16(
//              IDS_NOTIFICATION1_TITLE),
//          l10n_util::GetStringUTF16(
//              IDS_NOTIFICATION1_MESSAGE),
//          /*display_source=*/std::u16string(),
//          /*origin_url=*/GURL(),
//          message_center::NotifierId(
//              message_center::NotifierType::SYSTEM_COMPONENT,
//              kFoo,
//              NotificationCatalogName::kFooCatalog),
//          optional_data,
//          base::MakeRefCounted<
//              message_center::HandleNotificationClickDelegate>(
//                  base::BindRepeating(&OnClicked,
//                                      some_arg)),
//          kSettingsIcon,
//          message_center::SystemNotificationWarningLevel::WARNING);
// }
//
// void Foo::ShowNotification2() {
//   message_center::RichNotificationData optional_data;
//   optional_data.buttons.push_back(message_center::ButtonInfo(
//       l10n_util::GetStringUTF16(IDS_SOME_BUTTON)));
//
//    CreateSystemNotificationPtr(
//          message_center::NOTIFICATION_TYPE_SIMPLE,
//          kNotificationId2,
//          l10n_util::GetStringUTF16(
//              IDS_NOTIFICATION2_TITLE),
//          l10n_util::GetStringUTF16(
//              IDS_NOTIFICATION2_MESSAGE),
//          /*display_source=*/std::u16string(),
//          /*origin_url=*/GURL(),
//          message_center::NotifierId(
//              message_center::NotifierType::SYSTEM_COMPONENT,
//              kFoo,
//              NotificationCatalogName::kFooCatalog),
//          optional_data,
//          base::MakeRefCounted<
//              message_center::HandleNotificationClickDelegate>(
//                  base::BindRepeating(&OnClicked,
//                                      other_arg)),
//          kSettingsIcon,
//          message_center::SystemNotificationWarningLevel::WARNING);
// }
//
// Keep the builder as member and change only the fields that differ:
// Foo::Foo() {
//   message_center::RichNotificationData optional_data;
//   optional_data.buttons.push_back(message_center::ButtonInfo(
//       l10n_util::GetStringUTF16(IDS_SOME_BUTTON)));
//
//   builder_.SetNotifierId({
//         message_center::NotifierType::SYSTEM_COMPONENT,
//         kFoo,
//         NotificationCatalogName::kFooCatalog
//       }).SetOptionalFields(optional_data)
//       .SetSmallImage(kSettingsIcon)
//       .SetWarningLevel(
//       message_center::SystemNotificationWarningLevel::WARNING);
// }
//
// void Foo::ShowNotification1() {
//   builder_.SetId(kNotificationId1).SetTitleId(IDS_NOTIFICATION1_TITLE)
//     .SetMessageId(IDS_NOTIFICATION1_MESSAGE)
//     .SetDelegate(
//       base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
//           base::BindRepeating(&OnClicked,
//           some_arg)))
//     .Build(false);
// }
//
// void Foo::ShowNotification2() {
//   builder_.SetId(kNotificationId2).SetTitleId(IDS_NOTIFICATION2_TITLE)
//     .SetMessageId(IDS_NOTIFICATION2_MESSAGE)
//     .SetDelegate(
//       base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
//           base::BindRepeating(&OnClicked,
//           other_arg)))
//     .Build(false);
// }
//
// The builder can also be used when putting together the information for a
// notification and the information comes from several different branches
// depending on external factors. Instead of storing all the results in
// variables the corresponding builder methoder can be used directly.
//
// Example to reduce keeping uninitialized variables:
// void CreateNotification() {
//   std::u16string message;
//   if(SomeCondition()) {
//     message = l10n_util::GetStringFUTF16(IDS_MESSAGE_WITH_ARGS, GetArgs());
//   } else {
//     message = l10n_util::GetStringUTF16(IDS_MESSAGE_NO_ARGS);
//   }
//
//   AddNotification(CreateSystemNotificationPtr(
//     message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
//     kNotificationId,
//     l10n_util::GetStringUTF16(IDS_TITLE),
//     message,
//     ....
//   ));
//
// Create the notification like so:
// void CreateNotification() {
//   SystemNotificationBuilder builder;
//   if(SomeCondition()) {
//     builder.SetMessageWithArgs(IDS_MESSAGE_WITH_ARGS, GetArgs());
//   } else {
//     builder.SetMessageId(IDS_MESSAGE_NO_ARGS);
//   }
//
//   AddNotification(
//     builder.SetId(kNotificationId).SetTitle(IDS_TITLE).BuildPtr(false));
// }
class ASH_PUBLIC_EXPORT SystemNotificationBuilder {
 public:
  SystemNotificationBuilder();
  SystemNotificationBuilder(SystemNotificationBuilder&&);
  SystemNotificationBuilder& operator=(SystemNotificationBuilder&&);
  ~SystemNotificationBuilder();

  // Set the notification type.
  // Default: `message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE`.
  SystemNotificationBuilder& SetType(message_center::NotificationType type);

  // Change the ID of the notification. This method has to be called before
  // calling `Build()`/`BuildPtr()` with `id` being a non-empty value.
  SystemNotificationBuilder& SetId(const std::string& id);

  // Set a string title.
  // Default: ""
  SystemNotificationBuilder& SetTitle(const std::u16string& title);

  // Provide the IDS value for the title, it will be converted to a localized
  // string.
  SystemNotificationBuilder& SetTitleId(const int title_id);

  // Provide the IDS value for a title with replaceable arguments. The `args`
  // will be used to replace placeholders in the `title_id`.
  SystemNotificationBuilder& SetTitleWithArgs(
      const int title_id,
      const std::vector<std::u16string>& args);

  // Set a string as message/body.
  // Default: ""
  SystemNotificationBuilder& SetMessage(const std::u16string& message);

  // Provide the IDS value for the message/body, it will be converted to a
  // localized string.
  SystemNotificationBuilder& SetMessageId(const int message_id);

  // Provide the IDS value for a message/body with replaceable arguments. The
  // `args` will be used to replace placeholders in the `message_id`.
  SystemNotificationBuilder& SetMessageWithArgs(
      const int message_id,
      const std::vector<std::u16string>& args);

  // Set the display source.
  // Default: ""
  SystemNotificationBuilder& SetDisplaySource(
      const std::u16string& display_source);

  // Set the origin URL that requested the notification.
  // Default: Empty, invalid URL
  SystemNotificationBuilder& SetOriginUrl(const GURL& origin_url);

  // Set the notifier ID.
  // Default: Invalid NotifierId
  SystemNotificationBuilder& SetNotifierId(
      const message_center::NotifierId& notifier_id);

  // Set the catalog name of the NotifierId. This will generate a NotifierId
  // together with the value passed to `SetId()` when `Build()` is called.
  // Even if the catalog name is set through this method, any call to
  // `SetNotifierId()` will take precedence. Default: kNone
  SystemNotificationBuilder& SetCatalogName(
      NotificationCatalogName catalog_name);

  // Set the delegate that will receive events for this notification.
  // Default: nullptr
  SystemNotificationBuilder& SetDelegate(
      scoped_refptr<message_center::NotificationDelegate> delegate);

  // Set the small image shown in the notification.
  // Default: kNoneIcon
  SystemNotificationBuilder& SetSmallImage(const gfx::VectorIcon& small_image);

  // Set additional optional fields.
  // Default: Default constructed `message_center::RichNotificationData`
  SystemNotificationBuilder& SetOptionalFields(
      const message_center::RichNotificationData& optional_fields);

  // Returns currently set optional fields.
  const message_center::RichNotificationData& GetOptionalFields();

  // Set the warning level.
  // Default: `message_center::SystemNotificationWarningLevel::NORMAL`
  SystemNotificationBuilder& SetWarningLevel(
      message_center::SystemNotificationWarningLevel warning_level);

  // Create the notification from the currently stored fields.
  // Unless `keep_timestamp` is true, the `timestamp` field in the
  // `RichNotificationData` instance `optional_fields_` will be updated to the
  // current time inside `Build()`. Keeping the previous `timestamp` is useful
  // when `Build()` is used to update an existing notification.
  message_center::Notification Build(bool keep_timestamp);

  // Create a owning pointer of a notification from the currently stored fields.
  // Unless `keep_timestamp` is true, the `timestamp` field in the
  // `RichNotificationData` instance `optional_fields_` will be updated to the
  // current time inside `BuildPtr()`. Keeping the previous `timestamp` is
  // useful when `BuildPtr()` is used to update an existing notification.
  std::unique_ptr<message_center::Notification> BuildPtr(bool keep_timestamp);

  // Get a NotifierId by combining `catalog_name_` and `id_` if `notifier_id_`
  // is `std::nullopt`, otherwise returns the value of `notifier_id_`.
  // The `notifier_id_` should never be read directly but only through this
  // method.
  message_center::NotifierId GetNotifierId() const;

 private:
  message_center::NotificationType type_ =
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE;
  std::string id_;
  std::u16string title_;
  std::u16string message_;
  std::u16string display_source_;
  GURL origin_url_;
  std::optional<message_center::NotifierId> notifier_id_;
  NotificationCatalogName catalog_name_ = NotificationCatalogName::kNone;
  scoped_refptr<message_center::NotificationDelegate> delegate_ = nullptr;
  raw_ptr<const gfx::VectorIcon> small_image_ = &gfx::kNoneIcon;
  message_center::RichNotificationData optional_fields_;
  message_center::SystemNotificationWarningLevel warning_level_ =
      message_center::SystemNotificationWarningLevel::NORMAL;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_NOTIFICATION_BUILDER_H_
