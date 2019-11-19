// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_CHALLENGE_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_CHALLENGE_WRAPPER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"

namespace chromeos {

class EasyUnlockTpmKeyManager;

// Wraps a user challenge in a SecureMessage that can be verified by the remote
// device, containing the signature by the TPM of some unique data from the
// secure channel between the two devices.
class EasyUnlockChallengeWrapper {
 public:
  // Creates the instance:
  // |challenge|: The raw challenge to wrap.
  // |channel_binding_data|: Data unique to the current secure channel such that
  //                         we can bind with a TPM signature.
  // |account_id|: The id of the user who owns both devices.
  // |key_manager|: Responsible for signing some piece of data with the TPM.
  //                Not owned and should outlive this instance.
  EasyUnlockChallengeWrapper(const std::string& challenge,
                             const std::string& channel_binding_data,
                             const AccountId& account_id,
                             EasyUnlockTpmKeyManager* key_manager);
  virtual ~EasyUnlockChallengeWrapper();

  // Wraps the challenge and invokes |callback| with the |wrapped_challenge|
  // that will be send directly to the remote device.
  typedef base::Callback<void(const std::string& wrapped_challenge)>
      WrappedChallengeCallback;
  void WrapChallenge(const WrappedChallengeCallback& callback);

 protected:
  // Signs |data_to_sign| with the TPM. |callback| will be invoked upon
  // completion. Exposed for testing.
  virtual void SignUsingTpmKey(
      const std::string& data_to_sign,
      const base::Callback<void(const std::string&)>& callback);

 private:
  // Called when the channel binding data is signed by the TPM and completes the
  // wrapping.
  void OnChannelBindingDataSigned(const std::string& signature_metadata,
                                  const std::string& signature);

  // The raw challenge for the remote device.
  const std::string challenge_;

  // Data specific to the current secure channel to be signed by the TPM.
  const std::string channel_binding_data_;

  // The id of the user who owns both devices.
  const AccountId account_id_;

  // Responsible for signing data with the TPM. Not owned.
  EasyUnlockTpmKeyManager* key_manager_;

  // Called when wrapping completes.
  WrappedChallengeCallback callback_;

  base::WeakPtrFactory<EasyUnlockChallengeWrapper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockChallengeWrapper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_CHALLENGE_WRAPPER_H_
