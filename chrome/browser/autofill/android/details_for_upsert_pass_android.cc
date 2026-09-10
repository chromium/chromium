// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/details_for_upsert_pass_android.h"

#include "components/autofill/android/payments/legal_message_line_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/autofill/android/main_autofill_jni_headers/DetailsForUpsertPass_jni.h"

namespace jni_zero {

template <>
base::android::ScopedJavaLocalRef<jobject>
ToJniType<autofill::WalletPassAccessManager::GetDetailsForUpsertPassResponse>(
    JNIEnv* env,
    const autofill::WalletPassAccessManager::GetDetailsForUpsertPassResponse&
        response) {
  return autofill::Java_DetailsForUpsertPass_Constructor(
      env,
      autofill::LegalMessageLineAndroid::ConvertToJavaLinkedList(
          response.legal_message_lines),
      response.context_token);
}

}  // namespace jni_zero
