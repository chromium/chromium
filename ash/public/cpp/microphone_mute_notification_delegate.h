// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MICROPHONE_MUTE_NOTIFICATION_DELEGATE_H_
#define ASH_PUBLIC_CPP_MICROPHONE_MUTE_NOTIFICATION_DELEGATE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// This delegate exists so that code relevant to microphone mute notifications
// under //ash can call back into //chrome.  The actual delegate instance is
// owned and constructed by code in //chrome during startup.
class ASH_PUBLIC_EXPORT MicrophoneMuteNotificationDelegate {
 public:
  static MicrophoneMuteNotificationDelegate* Get();

  // Returns an optional string with:
  //
  // No value, if no app is accessing the mic
  // Empty value, if an app is accessing the mic but no name could be determined
  // Non-empty value, if an app is accessing the mic and a name could be
  // determined
  virtual absl::optional<std::u16string> GetAppAccessingMicrophone() = 0;

 protected:
  MicrophoneMuteNotificationDelegate();
  virtual ~MicrophoneMuteNotificationDelegate();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MICROPHONE_MUTE_NOTIFICATION_DELEGATE_H_
