// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_UTIL_HISTOGRAM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_UTIL_HISTOGRAM_UTIL_H_

namespace chromeos {
namespace secure_channel {
namespace util {

// Enumeration of possible message transfer action via Nearby Connection
// library. Keep in sync with corresponding enum in
// tools/metrics/histograms/enums.xml. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class MessageAction {
  kMessageSent = 0,
  kMessageReceived = 1,
  kMaxValue = kMessageReceived,
};

// Logs a given message transfer action.
void LogMessageAction(MessageAction message_action);

}  // namespace util
}  // namespace secure_channel
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_UTIL_HISTOGRAM_UTIL_H_
