// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/secure_channel/util/histogram_util.h"

#include "base/metrics/histogram_functions.h"

namespace chromeos {
namespace secure_channel {
namespace util {

void LogMessageAction(MessageAction message_action) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.MessageAction", message_action);
}

}  // namespace util
}  // namespace secure_channel
}  // namespace chromeos
