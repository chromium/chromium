// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/trusted_vault/trusted_vault_client_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "components/sync/service/sync_service_utils.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TrustedVaultClient_jni.h"

TrustedVaultClientAndroid::OngoingFetchKeys::OngoingFetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> callback)
    : account_info(account_info), callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingFetchKeys::OngoingFetchKeys(
    OngoingFetchKeys&&) = default;

TrustedVaultClientAndroid::OngoingFetchKeys::~OngoingFetchKeys() = default;

TrustedVaultClientAndroid::OngoingMarkLocalKeysAsStale::
    OngoingMarkLocalKeysAsStale(base::OnceCallback<void(bool)> callback)
    : callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingMarkLocalKeysAsStale::
    OngoingMarkLocalKeysAsStale(OngoingMarkLocalKeysAsStale&&) = default;

TrustedVaultClientAndroid::OngoingMarkLocalKeysAsStale::
    ~OngoingMarkLocalKeysAsStale() = default;

TrustedVaultClientAndroid::OngoingGetIsRecoverabilityDegraded::
    OngoingGetIsRecoverabilityDegraded(base::OnceCallback<void(bool)> callback)
    : callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingGetIsRecoverabilityDegraded::
    OngoingGetIsRecoverabilityDegraded(OngoingGetIsRecoverabilityDegraded&&) =
        default;

TrustedVaultClientAndroid::OngoingGetIsRecoverabilityDegraded::
    ~OngoingGetIsRecoverabilityDegraded() = default;

TrustedVaultClientAndroid::OngoingAddTrustedRecoveryMethod::
    OngoingAddTrustedRecoveryMethod(base::OnceClosure callback)
    : callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingAddTrustedRecoveryMethod::
    OngoingAddTrustedRecoveryMethod(OngoingAddTrustedRecoveryMethod&&) =
        default;

TrustedVaultClientAndroid::OngoingAddTrustedRecoveryMethod::
    ~OngoingAddTrustedRecoveryMethod() = default;

TrustedVaultClientAndroid::TrustedVaultClientAndroid(
    const GetAccountInfoByGaiaIdCallback& gaia_account_info_by_gaia_id_cb)
    : gaia_account_info_by_gaia_id_cb_(gaia_account_info_by_gaia_id_cb) {
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
    jint request_id,
    const base::android::JavaParamRef<jstring>& gaia_id,
    const base::android::JavaParamRef<jobjectArray>& keys) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OngoingRequest ongoing_request = GetAndUnregisterOngoingRequest(request_id);
  OngoingFetchKeys& ongoing_fetch_keys =
      absl::get<OngoingFetchKeys>(ongoing_request);

  DCHECK_EQ(ongoing_fetch_keys.account_info.gaia,
            base::android::ConvertJavaStringToUTF8(env, gaia_id))
      << "User mismatch in FetchKeys() response";

  std::vector<std::vector<uint8_t>> converted_keys;
  base::android::JavaArrayOfByteArrayToBytesVector(env, keys, &converted_keys);
  std::move(ongoing_fetch_keys.callback).Run(converted_keys);
}

void TrustedVaultClientAndroid::MarkLocalKeysAsStaleCompleted(
    JNIEnv* env,
    jint request_id,
    jboolean succeeded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OngoingRequest ongoing_request = GetAndUnregisterOngoingRequest(request_id);

  std::move(absl::get<OngoingMarkLocalKeysAsStale>(ongoing_request).callback)
      .Run(!!succeeded);
}

void TrustedVaultClientAndroid::GetIsRecoverabilityDegradedCompleted(
    JNIEnv* env,
    jint request_id,
    jboolean is_degraded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OngoingRequest ongoing_request = GetAndUnregisterOngoingRequest(request_id);

  std::move(
      absl::get<OngoingGetIsRecoverabilityDegraded>(ongoing_request).callback)
      .Run(!!is_degraded);
}

void TrustedVaultClientAndroid::AddTrustedRecoveryMethodCompleted(
    JNIEnv* env,
    jint request_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OngoingRequest ongoing_request = GetAndUnregisterOngoingRequest(request_id);

  std::move(
      absl::get<OngoingAddTrustedRecoveryMethod>(ongoing_request).callback)
      .Run();
}

void TrustedVaultClientAndroid::NotifyKeysChanged(JNIEnv* env) {
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultKeysChanged();
  }
}

void TrustedVaultClientAndroid::NotifyRecoverabilityChanged(JNIEnv* env) {
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultRecoverabilityChanged();
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

  // Store for later completion when Java invokes FetchKeysCompleted().
  const RequestId request_id =
      RegisterNewOngoingRequest(OngoingFetchKeys(account_info, std::move(cb)));

  JNIEnv* const env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jobject> java_account_info =
      ConvertToJavaCoreAccountInfo(env, account_info);

  // Trigger the fetching keys from the implementation in Java, which will
  // eventually call FetchKeysCompleted().
  Java_TrustedVaultClient_fetchKeys(env, reinterpret_cast<intptr_t>(this),
                                    request_id, java_account_info);
}

void TrustedVaultClientAndroid::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Not supported on Android, where keys are fetched outside the browser.
  NOTREACHED_IN_MIGRATION();
}

void TrustedVaultClientAndroid::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(cb);

  // Store for later completion when Java invokes
  // MarkLocalKeysAsStaleCompleted().
  const RequestId request_id =
      RegisterNewOngoingRequest(OngoingMarkLocalKeysAsStale(std::move(cb)));

  JNIEnv* const env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jobject> java_account_info =
      ConvertToJavaCoreAccountInfo(env, account_info);

  // The Java implementation will eventually call
  // MarkLocalKeysAsStaleCompleted().
  Java_TrustedVaultClient_markLocalKeysAsStale(
      env, reinterpret_cast<intptr_t>(this), request_id, java_account_info);
}

void TrustedVaultClientAndroid::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(cb);

  // Store for later completion when Java invokes
  // GetIsRecoverabilityDegradedCompleted().
  const RequestId request_id = RegisterNewOngoingRequest(
      OngoingGetIsRecoverabilityDegraded(std::move(cb)));

  JNIEnv* const env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jobject> java_account_info =
      ConvertToJavaCoreAccountInfo(env, account_info);

  // The Java implementation will eventually call
  // MarkLocalKeysAsStaleCompleted().
  Java_TrustedVaultClient_getIsRecoverabilityDegraded(
      env, reinterpret_cast<intptr_t>(this), request_id, java_account_info);
}

void TrustedVaultClientAndroid::AddTrustedRecoveryMethod(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(cb);

  const CoreAccountInfo account_info =
      gaia_account_info_by_gaia_id_cb_.Run(gaia_id);

  base::UmaHistogramBoolean(
      "Sync.TrustedVaultJavascriptAddRecoveryMethodUserKnown",
      account_info != CoreAccountInfo());

  if (account_info == CoreAccountInfo()) {
    std::move(cb).Run();
    return;
  }

  // Store for later completion when Java invokes
  // AddTrustedRecoveryMethodCompleted().
  const RequestId request_id =
      RegisterNewOngoingRequest(OngoingAddTrustedRecoveryMethod(std::move(cb)));

  JNIEnv* const env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jobject> java_account_info =
      ConvertToJavaCoreAccountInfo(env, account_info);

  const base::android::ScopedJavaLocalRef<jbyteArray> java_public_key =
      base::android::ToJavaByteArray(env, public_key);

  // The Java implementation will eventually call
  // AddTrustedRecoveryMethodCompleted().
  Java_TrustedVaultClient_addTrustedRecoveryMethod(
      env, reinterpret_cast<intptr_t>(this), request_id, java_account_info,
      java_public_key, method_type_hint);
}

void TrustedVaultClientAndroid::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  // Not relevant for Android implementation.
}

TrustedVaultClientAndroid::RequestId
TrustedVaultClientAndroid::RegisterNewOngoingRequest(OngoingRequest request) {
  ongoing_requests_.emplace(++last_request_id_, std::move(request));
  return last_request_id_;
}

TrustedVaultClientAndroid::OngoingRequest
TrustedVaultClientAndroid::GetAndUnregisterOngoingRequest(RequestId id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = ongoing_requests_.find(id);
  CHECK(it != ongoing_requests_.end(), base::NotFatalUntil::M130);

  OngoingRequest request = std::move(it->second);
  ongoing_requests_.erase(it);
  return request;
}

static void JNI_TrustedVaultClient_RecordKeyRetrievalTrigger(JNIEnv* env,
                                                             int trigger) {
  syncer::RecordKeyRetrievalTrigger(
      static_cast<syncer::TrustedVaultUserActionTriggerForUMA>(trigger));
}

static void JNI_TrustedVaultClient_RecordRecoverabilityDegradedFixTrigger(
    JNIEnv* env,
    int trigger) {
  syncer::RecordRecoverabilityDegradedFixTrigger(
      static_cast<syncer::TrustedVaultUserActionTriggerForUMA>(trigger));
}
