// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRUSTED_VAULT_TRUSTED_VAULT_CLIENT_ANDROID_H_
#define CHROME_BROWSER_TRUSTED_VAULT_TRUSTED_VAULT_CLIENT_ANDROID_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

// JNI bridge for a Java implementation of the TrustedVaultClient interface,
// used on Android.
//
// This class must be accessed from the UI thread.
// TODO(crbug.com/40915967): move this class under components/trusted_vault once
// its java counterpart does not have to stay under chrome/android (e.g. its
// dependecy on AppHooks is addressed).
class TrustedVaultClientAndroid : public trusted_vault::TrustedVaultClient {
 public:
  // Callback that returns account information identified by |gaia_id| or an
  // empty CoreAccountInfo if the account is not found.
  using GetAccountInfoByGaiaIdCallback =
      base::RepeatingCallback<CoreAccountInfo(const std::string& gaia_id)>;

  explicit TrustedVaultClientAndroid(
      const GetAccountInfoByGaiaIdCallback& gaia_account_info_by_gaia_id_cb);
  ~TrustedVaultClientAndroid() override;

  TrustedVaultClientAndroid(const TrustedVaultClientAndroid&) = delete;
  TrustedVaultClientAndroid& operator=(const TrustedVaultClientAndroid&) =
      delete;

  // Called from Java to notify the completion of a FetchKeys() operation
  // previously initiated from C++ and identified by |request_id|. |gaia_id|
  // must match the user's ID.
  void FetchKeysCompleted(
      JNIEnv* env,
      jint request_id,
      const base::android::JavaParamRef<jstring>& gaia_id,
      const base::android::JavaParamRef<jobjectArray>& keys);

  // Called from Java to notify the completion of a MarkLocalKeysAsStale()
  // operation previously initiated from C++ and identified by |request_id|.
  void MarkLocalKeysAsStaleCompleted(JNIEnv* env,
                                     jint request_id,
                                     jboolean succeeded);

  // Called from Java to notify the completion of a
  // GetIsRecoverabilityDegraded() operation previously initiated from C++ and
  // identified by |request_id|.
  void GetIsRecoverabilityDegradedCompleted(JNIEnv* env,
                                            jint request_id,
                                            jboolean is_degraded);

  // Called from Java to notify the completion of a
  // AddTrustedRecoveryMethod() operation previously initiated from C++ and
  // identified by |request_id|.
  void AddTrustedRecoveryMethodCompleted(JNIEnv* env, jint request_id);

  // Called from Java to notify that the keys in the vault may have changed.
  void NotifyKeysChanged(JNIEnv* env);

  // Called from Java to notify that the recoverability of the vault may have
  // changed.
  void NotifyRecoverabilityChanged(JNIEnv* env);

  // TrustedVaultClient implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void FetchKeys(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb)
      override;
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version) override;
  void MarkLocalKeysAsStale(const CoreAccountInfo& account_info,
                            base::OnceCallback<void(bool)> cb) override;
  void GetIsRecoverabilityDegraded(const CoreAccountInfo& account_info,
                                   base::OnceCallback<void(bool)> cb) override;
  void AddTrustedRecoveryMethod(const std::string& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint,
                                base::OnceClosure cb) override;
  void ClearLocalDataForAccount(const CoreAccountInfo& account_info) override;

 private:
  // Struct representing an in-flight FetchKeys() call invoked from C++.
  struct OngoingFetchKeys {
    OngoingFetchKeys(
        const CoreAccountInfo& account_info,
        base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
            callback);
    OngoingFetchKeys(OngoingFetchKeys&&);
    ~OngoingFetchKeys();

    const CoreAccountInfo account_info;
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> callback;
  };

  // Struct representing an in-flight MarkLocalKeysAsStale() call invoked from
  // C++.
  struct OngoingMarkLocalKeysAsStale {
    explicit OngoingMarkLocalKeysAsStale(
        base::OnceCallback<void(bool)> callback);
    OngoingMarkLocalKeysAsStale(OngoingMarkLocalKeysAsStale&&);
    ~OngoingMarkLocalKeysAsStale();

    base::OnceCallback<void(bool)> callback;
  };

  // Struct representing an in-flight GetIsRecoverabilityDegraded() invoked from
  // C++.
  struct OngoingGetIsRecoverabilityDegraded {
    explicit OngoingGetIsRecoverabilityDegraded(
        base::OnceCallback<void(bool)> callback);
    OngoingGetIsRecoverabilityDegraded(OngoingGetIsRecoverabilityDegraded&&);
    ~OngoingGetIsRecoverabilityDegraded();

    base::OnceCallback<void(bool)> callback;
  };

  // Struct representing an in-flight AddTrustedRecoveryMethod() invoked from
  // C++.
  struct OngoingAddTrustedRecoveryMethod {
    explicit OngoingAddTrustedRecoveryMethod(base::OnceClosure callback);
    OngoingAddTrustedRecoveryMethod(OngoingAddTrustedRecoveryMethod&&);
    ~OngoingAddTrustedRecoveryMethod();

    base::OnceClosure callback;
  };

  using RequestId = int32_t;
  using OngoingRequest = absl::variant<OngoingFetchKeys,
                                       OngoingMarkLocalKeysAsStale,
                                       OngoingGetIsRecoverabilityDegraded,
                                       OngoingAddTrustedRecoveryMethod>;

  RequestId RegisterNewOngoingRequest(OngoingRequest request);
  OngoingRequest GetAndUnregisterOngoingRequest(RequestId id);

  const GetAccountInfoByGaiaIdCallback gaia_account_info_by_gaia_id_cb_;

  // Each in-flight request gets assigned an auto-increment ID and gets cached
  // in a map, until the response from Java is received.
  RequestId last_request_id_ = 0;
  std::map<RequestId, OngoingRequest> ongoing_requests_;

  base::ObserverList<Observer> observer_list_;
};

#endif  // CHROME_BROWSER_TRUSTED_VAULT_TRUSTED_VAULT_CLIENT_ANDROID_H_
