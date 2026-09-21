// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/password_manager/android/utils_jni_headers/LoginDbDeprecationUtilBridge_jni.h"

namespace {

constexpr std::string_view kExportedPasswordsFileName = "ChromePasswords.csv";

}  // namespace

static std::string JNI_LoginDbDeprecationUtilBridge_GetAutoExportCsvFilePath(
    Profile* profile) {
  return profile->GetPath()
      .Append(FILE_PATH_LITERAL(kExportedPasswordsFileName))
      .value();
}

DEFINE_JNI(LoginDbDeprecationUtilBridge)
