// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TRUSTED_VAULT_CLIENT_ANDROID_H_
#define CHROME_BROWSER_SYNC_TRUSTED_VAULT_CLIENT_ANDROID_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/trusted_vault_client.h"

// JNI bridge for a Java implementation of the TrustedVaultClient interface,
// used on Android.
//
// This class must be accessed from the UI thread.
class TrustedVaultClientAndroid : public syncer::TrustedVaultClient {
 public:
  TrustedVaultClientAndroid();
  ~TrustedVaultClientAndroid() override;

  TrustedVaultClientAndroid(const TrustedVaultClientAndroid&) = delete;
  TrustedVaultClientAndroid& operator=(const TrustedVaultClientAndroid&) =
      delete;

  // Called from Java to notify the completion of a FetchKeys() operation
  // previously initiated from C++. This must correspond to an ongoing
  // FetchKeys() request, and |gaia_id| must match the user's ID.
  void FetchKeysCompleted(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& gaia_id,
      const base::android::JavaParamRef<jobjectArray>& keys);

  // Called from Java to notify the completion of MarkKeysAsStale()
  // operation previously initiated from C++. This must correspond to an
  // ongoing MarkKeysAsStale() request.
  void MarkKeysAsStaleCompleted(JNIEnv* env, jboolean result);

  // Called from Java to notify that the keys in the vault may have changed.
  void NotifyKeysChanged(JNIEnv* env);

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
  void RemoveAllStoredKeys() override;
  void MarkKeysAsStale(const CoreAccountInfo& account_info,
                       base::OnceCallback<void(bool)> cb) override;
  void GetIsRecoverabilityDegraded(const CoreAccountInfo& account_info,
                                   base::OnceCallback<void(bool)> cb) override;
  void AddTrustedRecoveryMethod(const std::string& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                base::OnceClosure cb) override;

 private:
  // Struct representing an in-flight FetchKeys() call invoked from C++.
  struct OngoingFetchKeys {
    OngoingFetchKeys(
        const CoreAccountInfo& account_info,
        base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
            callback);
    ~OngoingFetchKeys();

    const CoreAccountInfo account_info;
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> callback;
  };

  // Null if no in-flight FetchKeys().
  std::unique_ptr<OngoingFetchKeys> ongoing_fetch_keys_;

  // Completion callback of an in-flight MarkKeysAsStale() call invoked from
  // C++.
  base::OnceCallback<void(bool)> ongoing_mark_keys_as_stale_;

  base::ObserverList<Observer> observer_list_;
};

#endif  // CHROME_BROWSER_SYNC_TRUSTED_VAULT_CLIENT_ANDROID_H_
