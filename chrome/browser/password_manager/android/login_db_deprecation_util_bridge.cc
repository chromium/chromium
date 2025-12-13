// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <string_view>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/utils_jni_headers/LoginDbDeprecationUtilBridge_jni.h"

namespace {

constexpr std::string_view kExportedPasswordsFileName = "ChromePasswords.csv";

}  // namespace

static base::android::ScopedJavaLocalRef<jstring>
JNI_LoginDbDeprecationUtilBridge_GetAutoExportCsvFilePath(JNIEnv* env,
                                                          Profile* profile) {
  return base::android::ConvertUTF8ToJavaString(
      env, profile->GetPath()
               .Append(FILE_PATH_LITERAL(kExportedPasswordsFileName))
               .value());
}

DEFINE_JNI(LoginDbDeprecationUtilBridge)
