// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ANDROID_INTENT_HELPER_H_
#define ASH_PUBLIC_CPP_ANDROID_INTENT_HELPER_H_

#include <optional>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"

namespace ash {

// The class which allows to launch Android intent from Ash.
class ASH_PUBLIC_EXPORT AndroidIntentHelper {
 public:
  static AndroidIntentHelper* GetInstance();

  AndroidIntentHelper(const AndroidIntentHelper&) = delete;
  AndroidIntentHelper& operator=(const AndroidIntentHelper&) = delete;

  // Launch the given Android |intent|.
  virtual void LaunchAndroidIntent(const std::string& intent) = 0;

  // Get the intent that can be used to launch an Android activity specified by
  // the |app_info|.
  virtual std::optional<std::string> GetAndroidAppLaunchIntent(
      const assistant::AndroidAppInfo& app_info) = 0;

 protected:
  AndroidIntentHelper();
  virtual ~AndroidIntentHelper();
};

ASH_PUBLIC_EXPORT bool IsAndroidIntent(const GURL& url);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ANDROID_INTENT_HELPER_H_
