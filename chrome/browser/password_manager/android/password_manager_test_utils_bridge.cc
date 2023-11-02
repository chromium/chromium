// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include "chrome/browser/password_manager/android/test_support_jni_headers/PasswordManagerTestUtilsBridge_jni.h"
#include "components/password_manager/core/browser/password_form_manager.h"

// static
void JNI_PasswordManagerTestUtilsBridge_DisableServerPredictions(JNIEnv* env) {
  password_manager::PasswordFormManager::
      DisableFillingServerPredictionsForTesting();
}
