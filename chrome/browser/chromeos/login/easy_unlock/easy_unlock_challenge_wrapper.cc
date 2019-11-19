// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_challenge_wrapper.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/proto/securemessage.pb.h"

namespace chromeos {

namespace {

// Salt added to a SecureMessage.
const char kSalt[] =
    "\xbf\x9d\x2a\x53\xc6\x36\x16\xd7\x5d\xb0\xa7\x16\x5b\x91\xc1\xef\x73\xe5"
    "\x37\xf2\x42\x74\x05\xfa\x23\x61\x0a\x4b\xe6\x57\x64\x2e";

}  // namespace

EasyUnlockChallengeWrapper::EasyUnlockChallengeWrapper(
    const std::string& challenge,
    const std::string& channel_binding_data,
    const AccountId& account_id,
    EasyUnlockTpmKeyManager* key_manager)
    : challenge_(challenge),
      channel_binding_data_(channel_binding_data),
      account_id_(account_id),
      key_manager_(key_manager) {}

EasyUnlockChallengeWrapper::~EasyUnlockChallengeWrapper() {}

void EasyUnlockChallengeWrapper::WrapChallenge(
    const WrappedChallengeCallback& callback) {
  callback_ = callback;

  // Because the TPM is used to sign the channel binding data, we need to
  // construct the SecureMessage by hand instead of using the
  // SecureMessageDelegate.
  securemessage::Header header;
  header.set_signature_scheme(securemessage::RSA2048_SHA256);
  header.set_encryption_scheme(securemessage::NONE);
  header.set_associated_data_length(channel_binding_data_.length());

  securemessage::HeaderAndBody header_and_body;
  *(header_and_body.mutable_header()) = header;
  header_and_body.set_body(std::string());

  std::string signature_metadata = header_and_body.SerializeAsString();
  std::string data_to_sign =
      std::string(kSalt) + signature_metadata + channel_binding_data_;

  SignUsingTpmKey(
      data_to_sign,
      base::Bind(&EasyUnlockChallengeWrapper::OnChannelBindingDataSigned,
                 weak_ptr_factory_.GetWeakPtr(), signature_metadata));
}

void EasyUnlockChallengeWrapper::SignUsingTpmKey(
    const std::string& data_to_sign,
    const base::Callback<void(const std::string&)>& callback) {
  key_manager_->SignUsingTpmKey(account_id_, data_to_sign, callback);
}

void EasyUnlockChallengeWrapper::OnChannelBindingDataSigned(
    const std::string& signature_metadata,
    const std::string& signature) {
  // Wrap the challenge and channel binding signature in SecureMessage protos.
  securemessage::SecureMessage signature_container;
  signature_container.set_header_and_body(signature_metadata);
  signature_container.set_signature(signature);

  securemessage::SecureMessage wrapped_challenge;
  wrapped_challenge.set_header_and_body(challenge_);
  wrapped_challenge.set_signature(signature_container.SerializeAsString());

  PA_LOG(VERBOSE) << "Finished wrapping challenge.";
  callback_.Run(wrapped_challenge.SerializeAsString());
}

}  // namespace chromeos
