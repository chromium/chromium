// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/notification_launch_id.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/common/chrome_switches.h"

namespace {

enum LaunchIdComponents {
  NORMAL = 0,
  BUTTON_INDEX = 1,
  CONTEXT_MENU = 2,
  DISMISS_BUTTON = 3,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LaunchIdDecodeStatus {
  kSuccess = 0,
  kEmptyInput = 1,
  kComponentIdInvalid = 2,
  kComponentIdOutOfRange = 3,
  kTokensInsufficient = 4,
  kButtonIndexInvalid = 5,
  kTypeInvalid = 6,
  kTypeOutOfRange = 7,
  kMaxValue = kTypeOutOfRange,
};

void LogLaunchIdDecodeStatus(LaunchIdDecodeStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.LaunchIdDecodeStatus",
                            status);
}

}  // namespace

NotificationLaunchId::NotificationLaunchId() = default;

NotificationLaunchId::NotificationLaunchId(const NotificationLaunchId& other) =
    default;

NotificationLaunchId::NotificationLaunchId(
    NotificationHandler::Type notification_type,
    const std::string& notification_id,
    const std::string& profile_id,
    const std::wstring& app_user_model_id,
    bool incognito,
    const GURL& origin_url)
    : notification_type_(notification_type),
      notification_id_(notification_id),
      profile_id_(profile_id),
      app_user_model_id_(app_user_model_id),
      incognito_(incognito),
      origin_url_(origin_url),
      is_valid_(true) {}

NotificationLaunchId::NotificationLaunchId(const std::string& input) {
  if (input.empty()) {
    LogLaunchIdDecodeStatus(LaunchIdDecodeStatus::kEmptyInput);
    return;
  }

  const char kDelimiter[] = "|";
  std::vector<std::string> tokens = base::SplitString(
      input, kDelimiter, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // Figure out what kind of input string it is.
  int number;
  if (!base::StringToInt(tokens[0], &number)) {
    LogLaunchIdDecodeStatus(LaunchIdDecodeStatus::kComponentIdInvalid);
    return;
  }
  LaunchIdComponents components = static_cast<LaunchIdComponents>(number);

  // The final token may contain the separation character.
  size_t min_num_tokens;
  switch (components) {
    case NORMAL:
      // type|notification_type|profile_id|app_user_model_id|incognito|origin|
      // notification_id
      min_num_tokens = 7;
      break;
    case BUTTON_INDEX:
      // type|button_index|notification_type|profile_id|app_user_model_id|
      // incognito|origin|notification_id
      min_num_tokens = 8;
      break;
    case CONTEXT_MENU:
      // type|notification_type|profile_id|app_user_model_id|incognito|origin|
      // notification_id
      min_num_tokens = 7;
      is_for_context_menu_ = true;
      break;
    case DISMISS_BUTTON:
      // type|notification_type|profile_id|app_user_model_id|incognito|origin|
      // notification_id
      min_num_tokens = 7;
      is_for_dismiss_button_ = true;
      break;
    default:
      // |components| has an invalid value.
      LogLaunchIdDecodeStatus(LaunchIdDecodeStatus::kComponentIdOutOfRange);
      return;
  }

  if (tokens.size() < min_num_tokens) {
    LogLaunchIdDecodeStatus(LaunchIdDecodeStatus::kTokensInsufficient);
    return;
  }

  if (components == BUTTON_INDEX) {
    if (!base::StringToInt(tokens[1], &button_index_)) {
      LogLaunchIdDecodeStatus(LaunchIdDecodeStatus::kButtonIndexInvalid);
      return;
    }
    tokens.erase(tokens.begin());
  }

  int type = -1;
  if (!base::StringToInt(tokens[1], &type)) {
    LogLaunchIdDecodeStatus(LaunchIdDecodeStatus::kTypeInvalid);
    return;
  }
  if (type < 0 || type > static_cast<int>(NotificationHandler::Type::MAX)) {
    LogLaunchIdDecodeStatus(LaunchIdDecodeStatus::kTypeOutOfRange);
    return;
  }
  notification_type_ = static_cast<NotificationHandler::Type>(type);

  profile_id_ = tokens[2];
  app_user_model_id_ = base::UTF8ToWide(tokens[3]);
  incognito_ = tokens[4] == "1";
  origin_url_ = GURL(tokens[5]);

  notification_id_.clear();
  // Notification IDs is the rest of the string (delimiters not stripped off).
  const size_t kMinVectorSize = 6;
  for (size_t i = kMinVectorSize; i < tokens.size(); ++i) {
    if (i > kMinVectorSize)
      notification_id_ += kDelimiter;
    notification_id_ += tokens[i];
  }

  is_valid_ = true;
  LogLaunchIdDecodeStatus(LaunchIdDecodeStatus::kSuccess);
}

NotificationLaunchId::~NotificationLaunchId() = default;

std::string NotificationLaunchId::Serialize() const {
  // The pipe was chosen as delimiter because it is invalid for directory paths
  // and unsafe for origins -- and should therefore be encoded (as per
  // http://www.ietf.org/rfc/rfc1738.txt).
  std::string prefix;
  LaunchIdComponents type;
  if (is_for_context_menu_)
    type = CONTEXT_MENU;
  else if (is_for_dismiss_button_)
    type = DISMISS_BUTTON;
  else if (button_index_ > -1)
    type = BUTTON_INDEX;
  else
    type = NORMAL;

  if (button_index_ > -1)
    prefix = base::StringPrintf("|%d", button_index_);
  return base::StringPrintf(
      "%d%s|%d|%s|%s|%d|%s|%s", type, prefix.c_str(),
      static_cast<int>(notification_type_), profile_id_.c_str(),
      base::WideToUTF8(app_user_model_id_).c_str(), incognito_,
      origin_url_.spec().c_str(), notification_id_.c_str());
}

// static
std::string NotificationLaunchId::GetProfileIdFromLaunchId(
    const std::wstring& launch_id_str) {
  NotificationLaunchId launch_id(base::WideToUTF8(launch_id_str));

  // The launch_id_invalid failure is logged via HandleActivation(). We don't
  // re-log it here, which would skew the UMA failure metrics.
  return launch_id.is_valid() ? launch_id.profile_id() : std::string();
}

// static
base::FilePath NotificationLaunchId::GetNotificationLaunchProfileBaseName(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kNotificationLaunchId)) {
    return NotificationPlatformBridge::GetProfileBaseNameFromProfileId(
        NotificationLaunchId::GetProfileIdFromLaunchId(
            command_line.GetSwitchValueNative(
                switches::kNotificationLaunchId)));
  }
  return base::FilePath();
}
