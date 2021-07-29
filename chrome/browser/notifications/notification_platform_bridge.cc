// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

// static
std::string NotificationPlatformBridge::GetProfileId(Profile* profile) {
  if (!profile)
    return "";
#if defined(OS_WIN)
  return base::WideToUTF8(profile->GetBaseName().value());
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  return profile->GetBaseName().value();
#else
#error "Not implemented for !OS_WIN && !OS_POSIX."
#endif
}
