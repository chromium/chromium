// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_DETAILS_FOR_UPSERT_PASS_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_DETAILS_FOR_UPSERT_PASS_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "third_party/jni_zero/jni_zero.h"

namespace jni_zero {

template <>
base::android::ScopedJavaLocalRef<jobject>
ToJniType<autofill::WalletPassAccessManager::GetDetailsForUpsertPassResponse>(
    JNIEnv* env,
    const autofill::WalletPassAccessManager::GetDetailsForUpsertPassResponse&
        response);

}  // namespace jni_zero

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_DETAILS_FOR_UPSERT_PASS_ANDROID_H_
