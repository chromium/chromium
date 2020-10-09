// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/trusted_vault_client_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/android/chrome_jni_headers/TrustedVaultClient_jni.h"
#include "content/public/browser/browser_thread.h"

TrustedVaultClientAndroid::OngoingFetchKeys::OngoingFetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> callback)
    : account_info(account_info), callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingFetchKeys::~OngoingFetchKeys() = default;

TrustedVaultClientAndroid::TrustedVaultClientAndroid() {
  JNIEnv* const env = base::android::AttachCurrentThread();
  Java_TrustedVaultClient_registerNative(env, reinterpret_cast<intptr_t>(this));
}

TrustedVaultClientAndroid::~TrustedVaultClientAndroid() {
  JNIEnv* const env = base::android::AttachCurrentThread();
  Java_TrustedVaultClient_unregisterNative(env,
                                           reinterpret_cast<intptr_t>(this));
}

void TrustedVaultClientAndroid::FetchKeysCompleted(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& gaia_id,
    const base::android::JavaParamRef<jobjectArray>& keys) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ongoing_fetch_keys_) << "No ongoing FetchKeys() request";
  DCHECK_EQ(ongoing_fetch_keys_->account_info.gaia,
            base::android::ConvertJavaStringToUTF8(env, gaia_id))
      << "User mismatch in FetchKeys() response";

  // Make a copy of the callback and reset |ongoing_fetch_keys_| before invoking
  // the callback, in case it has side effects.
  base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb =
      std::move(ongoing_fetch_keys_->callback);
  ongoing_fetch_keys_.reset();

  std::vector<std::vector<uint8_t>> converted_keys;
  JavaArrayOfByteArrayToBytesVector(env, keys, &converted_keys);
  std::move(cb).Run(converted_keys);
}

void TrustedVaultClientAndroid::MarkKeysAsStaleCompleted(JNIEnv* env,
                                                         jboolean result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ongoing_mark_keys_as_stale_) << "No ongoing MarkKeysAsStale() request";

  // Make a copy of the callback and reset |ongoing_mark_keys_as_stale_| before
  // invoking the callback, in case it has side effects.
  base::OnceCallback<void(bool)> cb = std::move(ongoing_mark_keys_as_stale_);

  std::move(cb).Run(!!result);
}

void TrustedVaultClientAndroid::NotifyKeysChanged(JNIEnv* env) {
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultKeysChanged();
  }
}

void TrustedVaultClientAndroid::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TrustedVaultClientAndroid::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TrustedVaultClientAndroid::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!ongoing_fetch_keys_)
      << "Only one FetchKeys() request is allowed at any time";
  DCHECK(!ongoing_mark_keys_as_stale_)
      << "FetchKeys() not allowed while ongoing MarkKeysAsStale()";

  // Store for later completion when Java invokes FetchKeysCompleted().
  ongoing_fetch_keys_ =
      std::make_unique<OngoingFetchKeys>(account_info, std::move(cb));

  JNIEnv* const env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jobject> java_account_info =
      ConvertToJavaCoreAccountInfo(env, account_info);

  // Trigger the fetching keys from the implementation in Java, which will
  // eventually call FetchKeysCompleted().
  Java_TrustedVaultClient_fetchKeys(env, reinterpret_cast<intptr_t>(this),
                                    java_account_info);
}

void TrustedVaultClientAndroid::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Not supported on Android, where keys are fetched outside the browser.
  NOTREACHED();
}

void TrustedVaultClientAndroid::RemoveAllStoredKeys() {
  // StoreKeys() not supported on Android, nothing to remove.
}

void TrustedVaultClientAndroid::MarkKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(cb);
  DCHECK(!ongoing_mark_keys_as_stale_)
      << "Only one MarkKeysAsStale() request is allowed at any time";
  DCHECK(!ongoing_fetch_keys_)
      << "MarkKeysAsStale() not allowed while ongoing FetchKeys()";

  // Store for later completion when Java invokes MarkKeysAsStaleCompleted().
  ongoing_mark_keys_as_stale_ = std::move(cb);

  JNIEnv* const env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jobject> java_account_info =
      ConvertToJavaCoreAccountInfo(env, account_info);

  // The Java implementation will eventually call MarkKeysAsStaleCompleted().
  Java_TrustedVaultClient_markKeysAsStale(env, reinterpret_cast<intptr_t>(this),
                                          java_account_info);
}

void TrustedVaultClientAndroid::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  // TODO(crbug.com/1100279): Needs implementation.
  NOTIMPLEMENTED();
  std::move(cb).Run(false);
}

void TrustedVaultClientAndroid::AddTrustedRecoveryMethod(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    base::OnceClosure cb) {
  // TODO(crbug.com/1100279): Needs implementation.
  NOTIMPLEMENTED();
  std::move(cb).Run();
}
