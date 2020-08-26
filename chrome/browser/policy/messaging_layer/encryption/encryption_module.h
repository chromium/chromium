// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_ENCRYPTION_MODULE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_ENCRYPTION_MODULE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/encryption/encryption.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"

namespace reporting {

class EncryptionModule : public base::RefCountedThreadSafe<EncryptionModule> {
 public:
  // Feature to enable/disable encryption.
  // By default encryption is disabled, until server can support decryption.
  static const char kEncryptedReporting[];
  EncryptionModule();

  EncryptionModule(const EncryptionModule& other) = delete;
  EncryptionModule& operator=(const EncryptionModule& other) = delete;

  // EncryptRecord will attempt to encrypt the provided |record| and respond
  // with the callback. On success the returned EncryptedRecord will contain
  // the encrypted string and encryption information. EncryptedRecord then can
  // be further updated by the caller.
  virtual void EncryptRecord(
      base::StringPiece record,
      base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) const;

  // Records current public asymmetric key.
  virtual void UpdateAsymmetricKey(
      base::StringPiece new_key,
      base::OnceCallback<void(Status)> response_cb);

 protected:
  virtual ~EncryptionModule();

 private:
  friend base::RefCountedThreadSafe<EncryptionModule>;

  // Encryptor.
  scoped_refptr<Encryptor> encryptor_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_ENCRYPTION_MODULE_H_
