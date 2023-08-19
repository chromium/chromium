// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system_notification_builder.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

SystemNotificationBuilder::SystemNotificationBuilder() = default;
SystemNotificationBuilder::SystemNotificationBuilder(
    SystemNotificationBuilder&&) = default;
SystemNotificationBuilder& SystemNotificationBuilder::operator=(
    SystemNotificationBuilder&&) = default;
SystemNotificationBuilder::~SystemNotificationBuilder() = default;

SystemNotificationBuilder& SystemNotificationBuilder::SetType(
    message_center::NotificationType type) {
  type_ = type;
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetId(
    const std::string& id) {
  id_ = id;
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetTitle(
    const std::u16string& title) {
  title_ = title;
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetTitleId(
    const int title_id) {
  SetTitle(l10n_util::GetStringUTF16(title_id));
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetTitleWithArgs(
    const int title_id,
    const std::vector<std::u16string>& args) {
  SetTitle(l10n_util::GetStringFUTF16(title_id, args, /*offsets=*/nullptr));
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetMessage(
    const std::u16string& message) {
  message_ = message;
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetMessageId(
    const int message_id) {
  SetMessage(l10n_util::GetStringUTF16(message_id));
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetMessageWithArgs(
    const int message_id,
    const std::vector<std::u16string>& args) {
  SetMessage(l10n_util::GetStringFUTF16(message_id, args, /*offsets=*/nullptr));
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetDisplaySource(
    const std::u16string& display_source) {
  display_source_ = display_source;
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetOriginUrl(
    const GURL& origin_url) {
  origin_url_ = origin_url;
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetNotifierId(
    const message_center::NotifierId& notifier_id) {
  notifier_id_ = notifier_id;
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetCatalogName(
    const NotificationCatalogName catalog_name) {
  catalog_name_ = catalog_name;
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetDelegate(
    scoped_refptr<message_center::NotificationDelegate> delegate) {
  delegate_ = std::move(delegate);
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetSmallImage(
    const gfx::VectorIcon& small_image) {
  small_image_ = &small_image;
  return *this;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetOptionalFields(
    const message_center::RichNotificationData& optional_fields) {
  optional_fields_ = optional_fields;
  return *this;
}

const message_center::RichNotificationData&
SystemNotificationBuilder::GetOptionalFields() {
  return optional_fields_;
}

SystemNotificationBuilder& SystemNotificationBuilder::SetWarningLevel(
    message_center::SystemNotificationWarningLevel warning_level) {
  warning_level_ = warning_level;
  return *this;
}

message_center::Notification SystemNotificationBuilder::Build(
    bool keep_timestamp) {
  DCHECK(!id_.empty());
  const message_center::NotifierId notifier_id = GetNotifierId();
  DCHECK(notifier_id.type == message_center::NotifierType::SYSTEM_COMPONENT);
  if (!keep_timestamp) {
    optional_fields_.timestamp = base::Time::Now();
  }
  return CreateSystemNotification(type_, id_, title_, message_, display_source_,
                                  origin_url_, notifier_id, optional_fields_,
                                  delegate_, *small_image_, warning_level_);
}

std::unique_ptr<message_center::Notification>
SystemNotificationBuilder::BuildPtr(bool keep_timestamp) {
  return std::make_unique<message_center::Notification>(Build(keep_timestamp));
}

message_center::NotifierId SystemNotificationBuilder::GetNotifierId() const {
  // value_or doesn't work here as it eagerly constructs the potential
  // replacement value and the default `catalog_name_` is set to invalid
  // resulting in a failed DCHECK. This could be solved with the monadic
  // operations for optional but they won't be added until C++23.
  if (notifier_id_.has_value())
    return notifier_id_.value();

  return message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT, id_, catalog_name_);
}

}  // namespace ash
