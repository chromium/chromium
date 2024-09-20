// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <cert.h>
#include <certdb.h>
#include <cryptohi.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <pkcs11t.h>
#include <secder.h>
#include <secerr.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/net/client_cert_store_ash.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chromeos/ash/components/chaps_util/chaps_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_key_util.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/third_party/mozilla_security_manager/nsNSSCertificateDB.h"
#include "third_party/cros_system_api/constants/pkcs11_custom_attributes.h"

namespace ash::platform_keys {

void RunCallBackIfCallableElseRunCleanUp(base::OnceCallback<void()> callback,
                                         base::OnceCallback<void()> cleanup) {
  if (!callback.IsCancelled()) {
    return std::move(callback).Run();
  }
  if (!cleanup.IsCancelled()) {
    return std::move(cleanup).Run();
  }
  // else: TODO(b/280048774): Handle RemoveKey case when PlatformService
  // is not here.
}

namespace {

using ServiceWeakPtr = ::base::WeakPtr<PlatformKeysServiceImpl>;
using ::chromeos::platform_keys::HashAlgorithm;
using ::chromeos::platform_keys::KeyAttributeType;
using ::chromeos::platform_keys::KeyType;
using ::chromeos::platform_keys::OperationType;
using ::chromeos::platform_keys::Status;
using ::chromeos::platform_keys::SymKeyType;
using ::chromeos::platform_keys::TokenId;
using ::content::BrowserContext;
using ::content::BrowserThread;

// The current maximal RSA modulus length that ChromeOS's TPM supports for key
// generation.
const unsigned int kMaxRSAModulusLengthBits = 2048;

// Default constants for symmetric keys.
const int kDefaultSymKeySize = 32;
const int kDefaultSymSignatureLength = 32;

// Returns a vector containing bytes from `value` or an empty vector if `value`
// is nullptr.
std::vector<uint8_t> ScopedSECItemToBytes(const crypto::ScopedSECItem& value) {
  return value ? std::vector<uint8_t>(value->data, value->data + value->len)
               : std::vector<uint8_t>();
}

// Base class to store state that is common to all NSS database operations and
// to provide convenience methods to call back.
// Keeps track of the originating task runner.
class NSSOperationState {
 public:
  explicit NSSOperationState(ServiceWeakPtr weak_ptr)
      : service_weak_ptr_(weak_ptr) {}

  NSSOperationState(const NSSOperationState&) = delete;
  NSSOperationState& operator=(const NSSOperationState&) = delete;

  virtual ~NSSOperationState() = default;

  // Called if an error occurred during the execution of the NSS operation
  // described by this object.
  virtual void OnError(const base::Location& from, Status status) = 0;

  static void RunCallback(base::OnceClosure callback, ServiceWeakPtr weak_ptr) {
    if (weak_ptr) {
      std::move(callback).Run();
    }
  }
  crypto::ScopedPK11Slot slot_;

  // Weak pointer to the PlatformKeysServiceImpl that created this state. Used
  // to check if the callback should be still called.
  ServiceWeakPtr service_weak_ptr_;
};

using GetCertDBCallback =
    base::OnceCallback<void(net::NSSCertDatabase* cert_db)>;

// Called on the UI thread with certificate database.
void DidGetCertDbOnUiThread(std::optional<TokenId> token_id,
                            GetCertDBCallback callback,
                            NSSOperationState* state,
                            net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!cert_db) {
    LOG(ERROR) << "Couldn't get NSSCertDatabase.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  if (token_id) {
    switch (token_id.value()) {
      case TokenId::kUser:
        state->slot_ = cert_db->GetPrivateSlot();
        break;
      case TokenId::kSystem:
        state->slot_ = cert_db->GetSystemSlot();
        break;
    }

    if (!state->slot_) {
      LOG(ERROR) << "Slot for token id '" << static_cast<int>(token_id.value())
                 << "' not available.";
      state->OnError(FROM_HERE, Status::kErrorInternal);
      return;
    }
  }

  // Sets |slot_| of |state| accordingly and calls |callback| on the IO thread
  // if the database was successfully retrieved.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), cert_db));
}

// Asynchronously fetches the NSSCertDatabase using |delegate| and, if
// |token_id| is not empty, the slot for |token_id|. Stores the slot in |state|
// and passes the database to |callback|. Will run |callback| on the IO thread.
void GetCertDatabase(std::optional<TokenId> token_id,
                     GetCertDBCallback callback,
                     PlatformKeysServiceImplDelegate* delegate,
                     NSSOperationState* state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate->GetNSSCertDatabase(base::BindOnce(&DidGetCertDbOnUiThread, token_id,
                                              std::move(callback), state));
}

class GenerateRSAKeyState : public NSSOperationState {
 public:
  GenerateRSAKeyState(ServiceWeakPtr weak_ptr,
                      unsigned int modulus_length_bits,
                      bool sw_backed,
                      TokenId token_id,
                      GenerateKeyCallback callback)
      : NSSOperationState(weak_ptr),
        modulus_length_bits_(modulus_length_bits),
        sw_backed_(sw_backed),
        token_id_(token_id),
        callback_(std::move(callback)) {}

  ~GenerateRSAKeyState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, /*public_key_spki_der=*/std::vector<uint8_t>(), status);
  }

  void OnSuccess(const base::Location& from,
                 std::vector<uint8_t> public_key_spki_der) {
    CallBack(from, std::move(public_key_spki_der), Status::kSuccess);
  }

  const unsigned int modulus_length_bits_;
  const bool sw_backed_;
  TokenId token_id_;

 private:
  void CallBack(const base::Location& from,
                std::vector<uint8_t> public_key_spki_der,
                Status status) {
    auto success_callback =
        base::BindOnce(std::move(callback_), public_key_spki_der, status);
    // cleanup_callback will be called in case the main callback (callback_) is
    // canceled.
    auto cleanup_callback =
        base::BindOnce(&PlatformKeysServiceImpl::RemoveKey, service_weak_ptr_,
                       token_id_, public_key_spki_der, base::DoNothing());
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&RunCallBackIfCallableElseRunCleanUp,
                             std::move(success_callback),
                             std::move(cleanup_callback)));
  }

  // Must be called on origin thread, therefore use CallBack().
  GenerateKeyCallback callback_;
};

class GenerateECKeyState : public NSSOperationState {
 public:
  GenerateECKeyState(ServiceWeakPtr weak_ptr,
                     const std::string& named_curve,
                     TokenId token_id,
                     GenerateKeyCallback callback)
      : NSSOperationState(weak_ptr),
        named_curve_(std::move(named_curve)),
        token_id_(token_id),
        callback_(std::move(callback)) {}

  ~GenerateECKeyState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, /*public_key_spki_der=*/std::vector<uint8_t>(), status);
  }

  void OnSuccess(const base::Location& from,
                 std::vector<uint8_t> public_key_spki_der) {
    CallBack(from, std::move(public_key_spki_der), Status::kSuccess);
  }

  const std::string named_curve_;
  TokenId token_id_;

 private:
  void CallBack(const base::Location& from,
                std::vector<uint8_t> public_key_spki_der,
                Status status) {
    auto success_callback =
        base::BindOnce(std::move(callback_), public_key_spki_der, status);
    // cleanup_callback will be called in case the main callback (callback_) is
    // canceled.
    auto cleanup_callback =
        base::BindOnce(&PlatformKeysServiceImpl::RemoveKey, service_weak_ptr_,
                       token_id_, public_key_spki_der, base::DoNothing());
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&RunCallBackIfCallableElseRunCleanUp,
                             std::move(success_callback),
                             std::move(cleanup_callback)));
  }

  // Must be called on origin thread, therefore use CallBack().
  GenerateKeyCallback callback_;
};

class GenerateSymKeyState : public NSSOperationState {
 public:
  GenerateSymKeyState(ServiceWeakPtr weak_ptr,
                      std::vector<uint8_t> key_id,
                      int key_size,
                      SymKeyType key_type,
                      GenerateKeyCallback callback)
      : NSSOperationState(weak_ptr),
        key_id_(std::move(key_id)),
        key_size_(key_size),
        key_type_(key_type),
        callback_(std::move(callback)) {}

  ~GenerateSymKeyState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, /*key_id=*/std::vector<uint8_t>(), status);
  }

  void OnSuccess(const base::Location& from) {
    CallBack(from, key_id_, Status::kSuccess);
  }

  // This id is assigned to the newly created symmetric key.
  const std::vector<uint8_t> key_id_;

  const int key_size_;

  // Type of the key that determines which operations are allowed for it.
  const SymKeyType key_type_;

 private:
  void CallBack(const base::Location& from,
                std::vector<uint8_t> key_id,
                Status status) {
    auto bound_callback =
        base::BindOnce(std::move(callback_), std::move(key_id), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  GenerateKeyCallback callback_;
};

class EncryptDecryptState : public NSSOperationState {
 public:
  EncryptDecryptState(ServiceWeakPtr weak_ptr,
                      std::vector<uint8_t> input_data,
                      std::vector<uint8_t> key_id,
                      std::string algorithm,
                      std::vector<uint8_t> init_vector,
                      const OperationType operation_type,
                      EncryptDecryptCallback callback)
      : NSSOperationState(weak_ptr),
        input_data_(std::move(input_data)),
        key_id_(std::move(key_id)),
        algorithm_(std::move(algorithm)),
        init_vector_(std::move(init_vector)),
        operation_type_(operation_type),
        callback_(std::move(callback)) {}

  ~EncryptDecryptState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, /*output_data=*/std::vector<uint8_t>(), status);
  }

  void OnSuccess(const base::Location& from, std::vector<uint8_t> output_data) {
    CallBack(from, std::move(output_data), Status::kSuccess);
  }

  // The data that will be encrypted/decrypted.
  const std::vector<uint8_t> input_data_;

  // Symmetric key id.
  const std::vector<uint8_t> key_id_;

  // Determines the algorithm that is used to encrypt/decrypt.
  const std::string algorithm_;

  // Initialization vector that is required for encryption/decryption.
  // Must have a length of 16 bytes.
  const std::vector<uint8_t> init_vector_;

  // Specifies the operation, i.e. encryption or decryption.
  const OperationType operation_type_;

 private:
  void CallBack(const base::Location& from,
                std::vector<uint8_t> output_data,
                Status status) {
    auto bound_callback =
        base::BindOnce(std::move(callback_), std::move(output_data), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  EncryptDecryptCallback callback_;
};

class DeriveSymKeyState : public NSSOperationState {
 public:
  DeriveSymKeyState(ServiceWeakPtr weak_ptr,
                    std::vector<uint8_t> base_key_id,
                    std::vector<uint8_t> derived_key_id,
                    std::vector<uint8_t> label,
                    std::vector<uint8_t> context,
                    chromeos::platform_keys::SymKeyType derived_key_type,
                    DeriveKeyCallback callback,
                    bool kdf_counter_for_testing = false)
      : NSSOperationState(weak_ptr),
        base_key_id_(std::move(base_key_id)),
        derived_key_id_(std::move(derived_key_id)),
        label_(std::move(label)),
        context_(std::move(context)),
        derived_key_type_(derived_key_type),
        kdf_counter_for_testing_(kdf_counter_for_testing),
        callback_(std::move(callback)) {}

  ~DeriveSymKeyState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, /*key_id=*/std::vector<uint8_t>(), status);
  }

  void OnSuccess(const base::Location& from) {
    CallBack(from, derived_key_id_, Status::kSuccess);
  }

  // Id of the base key.
  const std::vector<uint8_t> base_key_id_;
  // Id of the new derived key.
  const std::vector<uint8_t> derived_key_id_;

  // Context and label are input parameters required for the derivation
  // algorithm.
  const std::vector<uint8_t> label_;
  const std::vector<uint8_t> context_;

  // Type of the key that determines which operations are allowed for it.
  const SymKeyType derived_key_type_;

  // Testing requires an additional parameter in form of kdf_counter.
  const bool kdf_counter_for_testing_ = false;

 private:
  void CallBack(const base::Location& from,
                std::vector<uint8_t> key_id,
                Status status) {
    auto bound_callback =
        base::BindOnce(std::move(callback_), std::move(key_id), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  DeriveKeyCallback callback_;
};

class SignState : public NSSOperationState {
 public:
  SignState(ServiceWeakPtr weak_ptr,
            std::vector<uint8_t> data,
            std::vector<uint8_t> public_key_spki_der,
            HashAlgorithm hash_algorithm,
            const KeyType key_type,
            SignCallback callback)
      : NSSOperationState(weak_ptr),
        data_(std::move(data)),
        public_key_spki_der_(std::move(public_key_spki_der)),
        hash_algorithm_(hash_algorithm),
        key_type_(key_type),
        callback_(std::move(callback)) {}

  ~SignState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, /*signature=*/std::vector<uint8_t>(), status);
  }

  void OnSuccess(const base::Location& from, std::vector<uint8_t> signature) {
    CallBack(from, std::move(signature), Status::kSuccess);
  }

  // The data that will be signed.
  const std::vector<uint8_t> data_;

  // Must be the DER encoding of a SubjectPublicKeyInfo.
  const std::vector<uint8_t> public_key_spki_der_;

  // Determines the hash algorithm that is used to digest |data| before signing.
  const HashAlgorithm hash_algorithm_;

  // Determines the type of the key that should be used for signing. This is
  // specified by the state creator.
  // Note: This can be different from the type of |public_key_spki_der|. In such
  // case, a runtime error should be thrown.
  const KeyType key_type_;

 private:
  void CallBack(const base::Location& from,
                std::vector<uint8_t> signature,
                Status status) {
    auto bound_callback =
        base::BindOnce(std::move(callback_), std::move(signature), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  SignCallback callback_;
};

class SignSymState : public NSSOperationState {
 public:
  SignSymState(ServiceWeakPtr weak_ptr,
               std::vector<uint8_t> data,
               std::vector<uint8_t> key_id,
               SignCallback callback)
      : NSSOperationState(weak_ptr),
        data_(std::move(data)),
        key_id_(std::move(key_id)),
        callback_(std::move(callback)) {}

  ~SignSymState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, /*signature=*/std::vector<uint8_t>(), status);
  }

  void OnSuccess(const base::Location& from, std::vector<uint8_t> signature) {
    CallBack(from, std::move(signature), Status::kSuccess);
  }

  // The data that will be signed.
  const std::vector<uint8_t> data_;

  // Symmetric key id.
  const std::vector<uint8_t> key_id_;

 private:
  void CallBack(const base::Location& from,
                std::vector<uint8_t> signature,
                Status status) {
    auto bound_callback =
        base::BindOnce(std::move(callback_), std::move(signature), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  SignCallback callback_;
};

class SelectCertificatesState : public NSSOperationState {
 public:
  SelectCertificatesState(
      ServiceWeakPtr weak_ptr,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
      SelectCertificatesCallback callback)
      : NSSOperationState(weak_ptr),
        cert_request_info_(cert_request_info),
        callback_(std::move(callback)) {}

  ~SelectCertificatesState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, std::unique_ptr<net::CertificateList>() /* no matches */,
             status);
  }

  void OnSuccess(const base::Location& from,
                 std::unique_ptr<net::CertificateList> matches) {
    CallBack(from, std::move(matches), Status::kSuccess);
  }

  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  std::unique_ptr<net::ClientCertStore> cert_store_;

 private:
  void CallBack(const base::Location& from,
                std::unique_ptr<net::CertificateList> matches,
                Status status) {
    auto bound_callback =
        base::BindOnce(std::move(callback_), std::move(matches), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  SelectCertificatesCallback callback_;
};

class GetCertificatesState : public NSSOperationState {
 public:
  GetCertificatesState(ServiceWeakPtr weak_ptr,
                       GetCertificatesCallback callback)
      : NSSOperationState(weak_ptr), callback_(std::move(callback)) {}

  ~GetCertificatesState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from,
             std::unique_ptr<net::CertificateList>() /* no certificates */,
             status);
  }

  void OnSuccess(const base::Location& from,
                 std::unique_ptr<net::CertificateList> certs) {
    CallBack(from, std::move(certs), Status::kSuccess);
  }

  net::ScopedCERTCertificateList certs_;

 private:
  void CallBack(const base::Location& from,
                std::unique_ptr<net::CertificateList> certs,
                Status status) {
    auto bound_callback =
        base::BindOnce(std::move(callback_), std::move(certs), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  GetCertificatesCallback callback_;
};

class GetAllKeysState : public NSSOperationState {
 public:
  GetAllKeysState(ServiceWeakPtr weak_ptr, GetAllKeysCallback callback)
      : NSSOperationState(weak_ptr), callback_(std::move(callback)) {}

  ~GetAllKeysState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from,
             /*public_key_spki_der_list=*/std::vector<std::vector<uint8_t>>(),
             status);
  }

  void OnSuccess(const base::Location& from,
                 std::vector<std::vector<uint8_t>> public_key_spki_der_list) {
    CallBack(from, std::move(public_key_spki_der_list), Status::kSuccess);
  }

 private:
  void CallBack(const base::Location& from,
                std::vector<std::vector<uint8_t>> public_key_spki_der_list,
                Status status) {
    auto bound_callback = base::BindOnce(
        std::move(callback_), std::move(public_key_spki_der_list), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  GetAllKeysCallback callback_;
};

class ImportCertificateState : public NSSOperationState {
 public:
  ImportCertificateState(ServiceWeakPtr weak_ptr,
                         const scoped_refptr<net::X509Certificate>& certificate,
                         ImportCertificateCallback callback)
      : NSSOperationState(weak_ptr),
        certificate_(certificate),
        callback_(std::move(callback)) {}

  ~ImportCertificateState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, status);
  }

  void OnSuccess(const base::Location& from) {
    CallBack(from, Status::kSuccess);
  }

  scoped_refptr<net::X509Certificate> certificate_;

 private:
  void CallBack(const base::Location& from, Status status) {
    auto bound_callback = base::BindOnce(std::move(callback_), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  ImportCertificateCallback callback_;
};

class RemoveCertificateState : public NSSOperationState {
 public:
  RemoveCertificateState(ServiceWeakPtr weak_ptr,
                         const scoped_refptr<net::X509Certificate>& certificate,
                         RemoveCertificateCallback callback)
      : NSSOperationState(weak_ptr),
        certificate_(certificate),
        callback_(std::move(callback)) {}

  ~RemoveCertificateState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, status);
  }

  void OnSuccess(const base::Location& from) {
    CallBack(from, Status::kSuccess);
  }

  scoped_refptr<net::X509Certificate> certificate_;

 private:
  void CallBack(const base::Location& from, Status status) {
    auto bound_callback = base::BindOnce(std::move(callback_), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  RemoveCertificateCallback callback_;
};

class RemoveKeyState : public NSSOperationState {
 public:
  RemoveKeyState(ServiceWeakPtr weak_ptr,
                 std::vector<uint8_t> public_key_spki_der,
                 RemoveKeyCallback callback)
      : NSSOperationState(weak_ptr),
        public_key_spki_der_(std::move(public_key_spki_der)),
        callback_(std::move(callback)) {}

  ~RemoveKeyState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, status);
  }

  void OnSuccess(const base::Location& from) {
    CallBack(from, Status::kSuccess);
  }

  // Must be a DER encoding of a SubjectPublicKeyInfo.
  const std::vector<uint8_t> public_key_spki_der_;

 private:
  void CallBack(const base::Location& from, Status status) {
    auto bound_callback = base::BindOnce(std::move(callback_), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  RemoveKeyCallback callback_;
};

class RemoveSymKeyState : public NSSOperationState {
 public:
  RemoveSymKeyState(ServiceWeakPtr weak_ptr,
                    std::vector<uint8_t> key_id,
                    RemoveKeyCallback callback)
      : NSSOperationState(weak_ptr),
        key_id_(std::move(key_id)),
        callback_(std::move(callback)) {}

  ~RemoveSymKeyState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, status);
  }

  void OnSuccess(const base::Location& from) {
    CallBack(from, Status::kSuccess);
  }

  // Symmetric key id.
  const std::vector<uint8_t> key_id_;

 private:
  void CallBack(const base::Location& from, Status status) {
    auto bound_callback = base::BindOnce(std::move(callback_), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  RemoveKeyCallback callback_;
};

class GetTokensState : public NSSOperationState {
 public:
  GetTokensState(ServiceWeakPtr weak_ptr, GetTokensCallback callback)
      : NSSOperationState(weak_ptr), callback_(std::move(callback)) {}

  ~GetTokensState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, std::vector<TokenId>() /* no token ids */, status);
  }

  void OnSuccess(const base::Location& from, std::vector<TokenId> token_ids) {
    CallBack(from, std::move(token_ids), Status::kSuccess);
  }

 private:
  void CallBack(const base::Location& from,
                std::vector<TokenId> token_ids,
                Status status) {
    auto bound_callback =
        base::BindOnce(std::move(callback_), std::move(token_ids), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  GetTokensCallback callback_;
};

class GetKeyLocationsState : public NSSOperationState {
 public:
  GetKeyLocationsState(ServiceWeakPtr weak_ptr,
                       std::vector<uint8_t> public_key_spki_der,
                       GetKeyLocationsCallback callback)
      : NSSOperationState(weak_ptr),
        public_key_spki_der_(std::move(public_key_spki_der)),
        callback_(std::move(callback)) {}

  ~GetKeyLocationsState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, /*token_ids=*/std::vector<TokenId>(), status);
  }

  void OnSuccess(const base::Location& from,
                 const std::vector<TokenId>& token_ids) {
    CallBack(from, token_ids, Status::kSuccess);
  }

  // Must be a DER encoding of a SubjectPublicKeyInfo.
  const std::vector<uint8_t> public_key_spki_der_;

 private:
  void CallBack(const base::Location& from,
                const std::vector<TokenId>& token_ids,
                Status status) {
    auto bound_callback =
        base::BindOnce(std::move(callback_), token_ids, status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  GetKeyLocationsCallback callback_;
};

class SetAttributeForKeyState : public NSSOperationState {
 public:
  SetAttributeForKeyState(ServiceWeakPtr weak_ptr,
                          std::vector<uint8_t> public_key_spki_der,
                          CK_ATTRIBUTE_TYPE attribute_type,
                          std::vector<uint8_t> attribute_value,
                          SetAttributeForKeyCallback callback)
      : NSSOperationState(weak_ptr),
        public_key_spki_der_(public_key_spki_der),
        attribute_type_(attribute_type),
        attribute_value_(std::move(attribute_value)),
        callback_(std::move(callback)) {}

  ~SetAttributeForKeyState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, status);
  }

  void OnSuccess(const base::Location& from) {
    CallBack(from, Status::kSuccess);
  }

  // Must be a DER encoding of a SubjectPublicKeyInfo.
  const std::vector<uint8_t> public_key_spki_der_;
  const CK_ATTRIBUTE_TYPE attribute_type_;
  const std::vector<uint8_t> attribute_value_;

 private:
  void CallBack(const base::Location& from, Status status) {
    auto bound_callback = base::BindOnce(std::move(callback_), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  SetAttributeForKeyCallback callback_;
};

class GetAttributeForKeyState : public NSSOperationState {
 public:
  GetAttributeForKeyState(ServiceWeakPtr weak_ptr,
                          std::vector<uint8_t> public_key_spki_der,
                          CK_ATTRIBUTE_TYPE attribute_type,
                          GetAttributeForKeyCallback callback)
      : NSSOperationState(weak_ptr),
        public_key_spki_der_(public_key_spki_der),
        attribute_type_(attribute_type),
        callback_(std::move(callback)) {}

  ~GetAttributeForKeyState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, /*attribute_value=*/std::nullopt, status);
  }

  void OnSuccess(const base::Location& from,
                 std::optional<std::vector<uint8_t>> attribute_value) {
    CallBack(from, std::move(attribute_value), Status::kSuccess);
  }

  // Must be a DER encoding of a SubjectPublicKeyInfo.
  const std::vector<uint8_t> public_key_spki_der_;
  const CK_ATTRIBUTE_TYPE attribute_type_;

 private:
  void CallBack(const base::Location& from,
                std::optional<std::vector<uint8_t>> attribute_value,
                Status status) {
    auto bound_callback = base::BindOnce(std::move(callback_),
                                         std::move(attribute_value), status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  GetAttributeForKeyCallback callback_;
};

class IsKeyOnTokenState : public NSSOperationState {
 public:
  IsKeyOnTokenState(ServiceWeakPtr weak_ptr,
                    std::vector<uint8_t> public_key_spki_der,
                    IsKeyOnTokenCallback callback)
      : NSSOperationState(weak_ptr),
        public_key_spki_der_(std::move(public_key_spki_der)),
        callback_(std::move(callback)) {}

  ~IsKeyOnTokenState() override = default;

  void OnError(const base::Location& from, Status status) override {
    CallBack(from, /*on_token=*/std::nullopt, status);
  }

  void OnSuccess(const base::Location& from, bool on_token) {
    CallBack(from, on_token, Status::kSuccess);
  }

  // Must be a DER encoding of a SubjectPublicKeyInfo.
  const std::vector<uint8_t> public_key_spki_der_;

 private:
  void CallBack(const base::Location& from,
                std::optional<bool> on_token,
                Status status) {
    auto bound_callback =
        base::BindOnce(std::move(callback_), on_token, status);
    content::GetUIThreadTaskRunner({})->PostTask(
        from, base::BindOnce(&NSSOperationState::RunCallback,
                             std::move(bound_callback), service_weak_ptr_));
  }

  // Must be called on origin thread, therefore use CallBack().
  IsKeyOnTokenCallback callback_;
};

// Returns the private key corresponding to the der-encoded
// |public_key_spki_der| if found in |slot|. If |slot| is nullptr, the private
// key will be searched in all slots.
crypto::ScopedSECKEYPrivateKey GetPrivateKey(
    const std::vector<uint8_t>& public_key_spki_der,
    PK11SlotInfo* slot) {
  if (slot) {
    return crypto::FindNSSKeyFromPublicKeyInfoInSlot(public_key_spki_der, slot);
  }
  return crypto::FindNSSKeyFromPublicKeyInfo(public_key_spki_der);
}

CK_MECHANISM_TYPE GetSymMechanismFromKeyType(const SymKeyType key_type) {
  switch (key_type) {
    case SymKeyType::kAesCbc:
      return CKM_AES_CBC_PAD;
    case SymKeyType::kHmac:
      return CKM_SHA256_HMAC;
    case SymKeyType::kSp800Kdf:
      return CKM_SP800_108_COUNTER_KDF;
  }
}

CK_ATTRIBUTE_TYPE GetSymOperationFromKeyType(const SymKeyType key_type) {
  switch (key_type) {
    case SymKeyType::kAesCbc:
      return CKA_ENCRYPT | CKA_DECRYPT;
    case SymKeyType::kHmac:
      return CKA_SIGN;
    case SymKeyType::kSp800Kdf:
      return CKA_DERIVE;
  }
}

// Returns the symmetric key with CKA_ID equal to |key_id|
// found in |slot|. |type| specifies the type of the key.
crypto::ScopedPK11SymKey GetSymKey(std::vector<uint8_t> key_id,
                                   PK11SlotInfo* slot,
                                   std::optional<SymKeyType> key_type) {
  SECItem sec_key_id{siUTF8String, key_id.data(),
                     static_cast<unsigned int>(key_id.size())};
  CK_MECHANISM_TYPE mechanism_type;
  if (!key_type) {
    mechanism_type = CKM_GENERIC_SECRET_KEY_GEN;
  } else {
    mechanism_type = GetSymMechanismFromKeyType(*key_type);
  }
  return crypto::ScopedPK11SymKey(PK11_FindFixedKey(slot, mechanism_type,
                                                    &sec_key_id,
                                                    /*wincx=*/nullptr));
}

// Does the actual symmetric key generation on a worker thread. Used by
// GenerateSymKeyWithDB().
void GenerateSymKeyOnWorkerThread(std::unique_ptr<GenerateSymKeyState> state) {
  if (!state->slot_) {
    LOG(ERROR) << "No slot.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  if (state->key_size_ != kDefaultSymKeySize) {
    LOG(ERROR) << "Only " << kDefaultSymKeySize << "-byte keys are supported.";
    state->OnError(FROM_HERE, Status::kErrorAlgorithmNotSupported);
    return;
  }

  crypto::ScopedPK11SymKey key =
      GetSymKey(state->key_id_, state->slot_.get(), std::nullopt);
  if (key) {
    LOG(ERROR)
        << "Cannot generate key because a key with given id already exists";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  std::vector<uint8_t> key_id = state->key_id_;
  SECItem sec_key_id{siUTF8String, key_id.data(),
                     static_cast<unsigned int>(key_id.size())};
  CK_FLAGS op_flags;
  CK_MECHANISM_TYPE key_type;

  switch (state->key_type_) {
    case SymKeyType::kAesCbc:
      op_flags = CKF_ENCRYPT | CKF_DECRYPT;
      key_type = CKM_AES_KEY_GEN;
      break;
    case SymKeyType::kHmac:
      op_flags = CKF_SIGN;
      key_type = CKM_SHA256_HMAC;
      break;
    case SymKeyType::kSp800Kdf:
      op_flags = CKF_DERIVE;
      key_type = CKM_SP800_108_COUNTER_KDF;
      break;
  }

  if (!PK11_TokenKeyGenWithFlags(
          state->slot_.get(), key_type,
          /*param=*/nullptr, state->key_size_, &sec_key_id, op_flags,
          /*attr_flags=*/PK11_ATTR_TOKEN | PK11_ATTR_PRIVATE,
          /*wincx=*/nullptr)) {
    LOG(ERROR) << "Couldn't generate symmetric key.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }
  state->OnSuccess(FROM_HERE);
}

// Continues generating a symmetric key with the obtained NSSCertDatabase. Used
// by GenerateSymKey().
void GenerateSymKeyWithDB(std::unique_ptr<GenerateSymKeyState> state,
                          net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GenerateSymKeyOnWorkerThread, std::move(state)));
}

// Does the actual RSA key generation on a worker thread. Used by
// GenerateRSAKeyWithDB().
void GenerateRSAKeyOnWorkerThread(std::unique_ptr<GenerateRSAKeyState> state) {
  if (!state->slot_) {
    LOG(ERROR) << "No slot.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;

  bool key_gen_success;
  if (state->sw_backed_) {
    auto chaps_util = chromeos::ChapsUtil::Create();
    key_gen_success = chaps_util->GenerateSoftwareBackedRSAKey(
        state->slot_.get(), state->modulus_length_bits_, &public_key,
        &private_key);
  } else {
    key_gen_success = crypto::GenerateRSAKeyPairNSS(
        state->slot_.get(), state->modulus_length_bits_, true /* permanent */,
        &public_key, &private_key);
  }
  if (!key_gen_success) {
    LOG(ERROR) << "Couldn't create key, sw_backed=" << state->sw_backed_;
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key.get()));
  if (!public_key_der) {
    // TODO(crbug.com/40115571): Remove private_key and public_key from
    // storage.
    LOG(ERROR) << "Couldn't export public key.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }
  state->OnSuccess(FROM_HERE, ScopedSECItemToBytes(public_key_der));
}

// Does the actual EC key generation on a worker thread. Used by
// GenerateECKeyWithDB().
void GenerateECKeyOnWorkerThread(std::unique_ptr<GenerateECKeyState> state) {
  if (!state->slot_) {
    LOG(ERROR) << "No slot.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  if (state->named_curve_ != "P-256") {
    LOG(ERROR) << "Only P-256 named curve is supported.";
    state->OnError(FROM_HERE, Status::kErrorAlgorithmNotSupported);
  }

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  if (!crypto::GenerateECKeyPairNSS(
          state->slot_.get(), SEC_OID_ANSIX962_EC_PRIME256V1,
          true /* permanent */, &public_key, &private_key)) {
    LOG(ERROR) << "Couldn't create key.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key.get()));
  if (!public_key_der) {
    // TODO(crbug.com/40115571): Remove private_key and public_key from
    // storage.
    LOG(ERROR) << "Couldn't export public key.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }
  state->OnSuccess(FROM_HERE, ScopedSECItemToBytes(public_key_der));
}

// Continues generating a RSA key with the obtained NSSCertDatabase. Used by
// GenerateRSAKey().
void GenerateRSAKeyWithDB(std::unique_ptr<GenerateRSAKeyState> state,
                          net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GenerateRSAKeyOnWorkerThread, std::move(state)));
}

// Continues generating an EC key with the obtained NSSCertDatabase. Used by
// GenerateECKey().
void GenerateECKeyWithDB(std::unique_ptr<GenerateECKeyState> state,
                         net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GenerateECKeyOnWorkerThread, std::move(state)));
}

// Does the actual AES encryption/decryption on a worker thread.
// Used by EncryptDecryptAESWithDB().
void EncryptDecryptAESOnWorkerThread(
    std::unique_ptr<EncryptDecryptState> state) {
  if (!state->slot_) {
    LOG(ERROR) << "No slot.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  if (state->algorithm_ != "AES-CBC") {
    LOG(ERROR) << "Only AES-CBC encryption is supported.";
    state->OnError(FROM_HERE, Status::kErrorAlgorithmNotSupported);
    return;
  }

  std::vector<uint8_t> iv = state->init_vector_;
  if (iv.size() != 16) {
    LOG(ERROR) << "Initialization vector's length should be equal to 16.";
    state->OnError(FROM_HERE, Status::kErrorAlgorithmNotSupported);
    return;
  }

  crypto::ScopedPK11SymKey key =
      GetSymKey(state->key_id_, state->slot_.get(), SymKeyType::kAesCbc);
  if (!key) {
    LOG(ERROR) << "Couldn't find the symmetric key.";
    state->OnError(FROM_HERE, Status::kErrorKeyNotFound);
    return;
  }

  SECItem sec_iv{siUTF8String, iv.data(), static_cast<unsigned int>(iv.size())};
  // Input string might be padded so its length would be a multiple of 16, which
  // means the encrypted string could be larger than the initial one.
  std::vector<uint8_t> result(state->input_data_.size() + 16);
  unsigned int result_len = 0;

  switch (state->operation_type_) {
    case OperationType::kEncrypt:
      if (PK11_Encrypt(key.get(), PK11_GetMechanism(key.get()), &sec_iv,
                       result.data(), &result_len, result.size(),
                       state->input_data_.data(),
                       state->input_data_.size()) != SECSuccess) {
        LOG(ERROR) << "Encryption failed.";
        state->OnError(FROM_HERE, Status::kErrorInternal);
        return;
      }
      break;
    case OperationType::kDecrypt:
      if (PK11_Decrypt(key.get(), PK11_GetMechanism(key.get()), &sec_iv,
                       result.data(), &result_len, result.size(),
                       state->input_data_.data(),
                       state->input_data_.size()) != SECSuccess) {
        LOG(ERROR) << "Decryption failed.";
        state->OnError(FROM_HERE, Status::kErrorInternal);
        return;
      }
      break;
  }

  result.resize(result_len);
  state->OnSuccess(FROM_HERE, std::move(result));
}

// Continues AES encryption/decryption with the obtained NSSCertDatabase.
// Used by EncryptDecryptAES().
void EncryptDecryptAESWithDB(std::unique_ptr<EncryptDecryptState> state,
                             net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&EncryptDecryptAESOnWorkerThread, std::move(state)));
}

// Does the actual key derivation on a worker thread.
// Used by DeriveSymKeyWithDB.
void DeriveSymKeyOnWorkerThread(std::unique_ptr<DeriveSymKeyState> state) {
  if (!state->slot_) {
    LOG(ERROR) << "No slot.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  crypto::ScopedPK11SymKey base_key =
      GetSymKey(state->base_key_id_, state->slot_.get(), SymKeyType::kSp800Kdf);
  if (!base_key) {
    LOG(ERROR) << "Couldn't find the symmetric base key.";
    state->OnError(FROM_HERE, Status::kErrorKeyNotFound);
    return;
  }

  std::vector<uint8_t> derived_key_id = state->derived_key_id_;
  if (crypto::ScopedPK11SymKey key =
          GetSymKey(derived_key_id, state->slot_.get(), std::nullopt);
      key) {
    LOG(ERROR)
        << "Cannot derive key because a key with given id already exists";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  std::vector<uint8_t> label = state->label_;
  std::vector<uint8_t> context = state->context_;
  std::vector<CK_PRF_DATA_PARAM> data_params{
      {CK_SP800_108_BYTE_ARRAY, label.data(),
       static_cast<unsigned int>(label.size())},
      {CK_SP800_108_BYTE_ARRAY, context.data(),
       static_cast<unsigned int>(context.size())}};
  // Derivation algorithm used in testing accepts slightly different parameters.
  CK_SP800_108_COUNTER_FORMAT ck_counter{CK_TRUE, 16};
  if (state->kdf_counter_for_testing_) {
    data_params.push_back(
        {CK_SP800_108_ITERATION_VARIABLE, &ck_counter, sizeof(ck_counter)});
  }
  // prfType specifies the deriving algorithm and only CKM_SHA256_HMAC is
  // currently implemented in chaps.
  CK_SP800_108_KDF_PARAMS kdf_params = {
      /*prfType=*/CKM_SHA256_HMAC, data_params.size(), data_params.data(),
      /*ulAdditionalDerivedKeys=*/0, /*pAdditionalDerivedKeys=*/nullptr};
  SECItem param{siUTF8String, reinterpret_cast<uint8_t*>(&kdf_params),
                static_cast<unsigned int>(sizeof(CK_SP800_108_KDF_PARAMS))};

  CK_BBOOL ck_true = CK_TRUE;
  std::vector<CK_ATTRIBUTE> attrs = {
      {CKA_ID, derived_key_id.data(),
       static_cast<unsigned int>(derived_key_id.size())},
      {CKA_TOKEN, &ck_true, sizeof(ck_true)}};

  if (!PK11_DeriveWithTemplate(
          base_key.get(), PK11_GetMechanism(base_key.get()), &param,
          GetSymMechanismFromKeyType(state->derived_key_type_),
          GetSymOperationFromKeyType(state->derived_key_type_),
          kDefaultSymKeySize, attrs.data(), attrs.size(), /*isPerm=*/PR_TRUE)) {
    LOG(ERROR) << "Couldn't derive symmetric key.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }
  state->OnSuccess(FROM_HERE);
}

// Continues key derivation with the obtained NSSCertDatabase.
// Used by DeriveSymKey().
void DeriveSymKeyWithDB(std::unique_ptr<DeriveSymKeyState> state,
                        net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DeriveSymKeyOnWorkerThread, std::move(state)));
}

// Checks whether |input_length| is lower or equal to the maximum input length
// for a RSA PKCS#1 v1.5 signature generated using |private_key| with PK11_Sign.
// Returns false if |input_length| is too large.
// If the maximum input length can not be determined (which is possible because
// it queries the PKCS#11 module), returns true and logs a warning.
bool CheckRSAPKCS1SignRawInputLength(SECKEYPrivateKey* private_key,
                                     size_t input_length) {
  // For RSA keys, PK11_Sign will perform PKCS#1 v1.5 padding, which needs at
  // least 11 bytes. RSA Sign can process an input of max. modulus length.
  // Thus the maximum input length for the sign operation is
  // |modulus_length - 11|.
  int modulus_length_bytes = PK11_GetPrivateModulusLen(private_key);
  if (modulus_length_bytes <= 0) {
    LOG(WARNING) << "Could not determine modulus length";
    return true;
  }
  size_t max_input_length_after_padding =
      static_cast<size_t>(modulus_length_bytes);
  // PKCS#1 v1.5 Padding needs at least this many bytes.
  size_t kMinPaddingLength = 11u;
  return input_length + kMinPaddingLength <= max_input_length_after_padding;
}

// Performs "raw" PKCS1 v1.5 padding + signing by calling PK11_Sign on
// |rsa_key|.
void SignRSAPKCS1RawOnWorkerThread(std::unique_ptr<SignState> state,
                                   crypto::ScopedSECKEYPrivateKey rsa_key) {
  static_assert(
      sizeof(*state->data_.data()) == sizeof(char),
      "Can't reinterpret data if it's characters are not 8 bit large.");
  SECItem input = {siBuffer, const_cast<unsigned char*>(state->data_.data()),
                   static_cast<unsigned int>(state->data_.size())};

  // Compute signature of hash.
  int signature_len = PK11_SignatureLen(rsa_key.get());
  if (signature_len <= 0) {
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  std::vector<unsigned char> signature(signature_len);
  SECItem signature_output = {siBuffer, signature.data(),
                              static_cast<unsigned int>(signature.size())};
  if (PK11_Sign(rsa_key.get(), &signature_output, &input) != SECSuccess) {
    // Input size is checked after a failure - obtaining max input size
    // involves extracting key modulus length which is not a free operation, so
    // don't bother if signing succeeded.
    // Note: It would be better if this could be determined from some library
    // return code (e.g. PORT_GetError), but this was not possible with
    // NSS+chaps at this point.
    if (!CheckRSAPKCS1SignRawInputLength(rsa_key.get(), state->data_.size())) {
      LOG(ERROR) << "Couldn't sign - input too long.";
      state->OnError(FROM_HERE, Status::kErrorInputTooLong);
      return;
    }
    LOG(ERROR) << "Couldn't sign.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }
  state->OnSuccess(FROM_HERE, std::move(signature));
}

// Does the actual RSA signing on a worker thread.
void SignRSAOnWorkerThread(std::unique_ptr<SignState> state) {
  crypto::ScopedSECKEYPrivateKey rsa_key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_.get());

  // Fail if the key was not found or is of the wrong type.
  if (!rsa_key || SECKEY_GetPrivateKeyType(rsa_key.get()) != rsaKey) {
    state->OnError(FROM_HERE, Status::kErrorKeyNotFound);
    return;
  }

  if (state->hash_algorithm_ == HashAlgorithm::HASH_ALGORITHM_NONE) {
    SignRSAPKCS1RawOnWorkerThread(std::move(state), std::move(rsa_key));
    return;
  }

  SECOidTag sign_alg_tag = SEC_OID_UNKNOWN;
  switch (state->hash_algorithm_) {
    case HashAlgorithm::HASH_ALGORITHM_SHA1:
      sign_alg_tag = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
      break;
    case HashAlgorithm::HASH_ALGORITHM_SHA256:
      sign_alg_tag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
      break;
    case HashAlgorithm::HASH_ALGORITHM_SHA384:
      sign_alg_tag = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
      break;
    case HashAlgorithm::HASH_ALGORITHM_SHA512:
      sign_alg_tag = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
      break;
    case HashAlgorithm::HASH_ALGORITHM_NONE:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  crypto::ScopedSECItem sign_result(SECITEM_AllocItem(nullptr, nullptr, 0));
  if (SEC_SignData(sign_result.get(),
                   reinterpret_cast<const unsigned char*>(state->data_.data()),
                   state->data_.size(), rsa_key.get(),
                   sign_alg_tag) != SECSuccess) {
    LOG(ERROR) << "Couldn't sign.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  state->OnSuccess(FROM_HERE, ScopedSECItemToBytes(sign_result));
}

// Does the actual EC Signing on a worker thread.
void SignECOnWorkerThread(std::unique_ptr<SignState> state) {
  crypto::ScopedSECKEYPrivateKey ec_key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_.get());

  // Fail if the key was not found or is of the wrong type.
  if (!ec_key || SECKEY_GetPrivateKeyType(ec_key.get()) != ecKey) {
    state->OnError(FROM_HERE, Status::kErrorKeyNotFound);
    return;
  }

  DCHECK(state->hash_algorithm_ != HashAlgorithm::HASH_ALGORITHM_NONE);

  // Only SHA-256 algorithm is supported for ECDSA.
  if (state->hash_algorithm_ != HashAlgorithm::HASH_ALGORITHM_SHA256) {
    state->OnError(FROM_HERE, Status::kErrorAlgorithmNotSupported);
    return;
  }

  std::string signature_str;
  SECOidTag sign_alg_tag = SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE;
  crypto::ScopedSECItem sign_result(SECITEM_AllocItem(nullptr, nullptr, 0));
  if (SEC_SignData(sign_result.get(),
                   reinterpret_cast<const unsigned char*>(state->data_.data()),
                   state->data_.size(), ec_key.get(),
                   sign_alg_tag) != SECSuccess) {
    LOG(ERROR) << "Couldn't sign using elliptic curve key.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  int signature_len = PK11_SignatureLen(ec_key.get());
  crypto::ScopedSECItem web_crypto_signature(
      DSAU_DecodeDerSigToLen(sign_result.get(), signature_len));

  if (!web_crypto_signature || web_crypto_signature->len == 0) {
    LOG(ERROR) << "Couldn't convert signature to WebCrypto format.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  state->OnSuccess(FROM_HERE, ScopedSECItemToBytes(web_crypto_signature));
}

// Decides which signing algorithm will be used. Used by SignWithDB().
void SignOnWorkerThread(std::unique_ptr<SignState> state) {
  crypto::ScopedSECKEYPrivateKey key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_.get());

  if (!key) {
    state->OnError(FROM_HERE, Status::kErrorKeyNotFound);
    return;
  }

  switch (state->key_type_) {
    case KeyType::kRsassaPkcs1V15:
      SignRSAOnWorkerThread(std::move(state));
      break;
    case KeyType::kEcdsa:
      SignECOnWorkerThread(std::move(state));
      break;
  }
}

// Continues signing with the obtained NSSCertDatabase. Used by Sign().
void SignWithDB(std::unique_ptr<SignState> state,
                net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SignOnWorkerThread, std::move(state)));
}

// Does the actual SHA-256 HMAC signing on a worker thread.
// Used by SignSymWithDB().
void SignSymOnWorkerThread(std::unique_ptr<SignSymState> state) {
  if (!state->slot_) {
    LOG(ERROR) << "No slot.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  crypto::ScopedPK11SymKey key =
      GetSymKey(state->key_id_, state->slot_.get(), SymKeyType::kHmac);
  if (!key) {
    LOG(ERROR) << "Couldn't find the symmetric key.";
    state->OnError(FROM_HERE, Status::kErrorKeyNotFound);
    return;
  }

  std::vector<uint8_t> signature(kDefaultSymSignatureLength);
  std::vector<uint8_t> data = state->data_;
  SECItem sec_signature{siUTF8String, signature.data(),
                        static_cast<unsigned int>(signature.size())};
  SECItem sec_data{siUTF8String, data.data(),
                   static_cast<unsigned int>(data.size())};

  if (PK11_SignWithSymKey(key.get(), PK11_GetMechanism(key.get()),
                          /*param=*/nullptr, &sec_signature,
                          &sec_data) != SECSuccess) {
    LOG(ERROR) << "Signing failed.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  state->OnSuccess(FROM_HERE, std::move(signature));
}

// Continues signing with the obtained NSSCertDatabase. Used by
// SignWithSymKey().
void SignSymWithDB(std::unique_ptr<SignSymState> state,
                   net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SignSymOnWorkerThread, std::move(state)));
}

// Called when `ClientCertStoreAsh::GetClientCerts` is done. Builds the list of
// `net::CertificateList` and calls back. Used by `SelectCertificates()`.
void DidSelectCertificates(std::unique_ptr<SelectCertificatesState> state,
                           net::ClientCertIdentityList identities) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Convert the ClientCertIdentityList to a CertificateList since returning
  // ClientCertIdentities would require changing the platformKeys extension
  // api. This assumes that the necessary keys can be found later with
  // crypto::FindNSSKeyFromPublicKeyInfo.
  auto certs = std::make_unique<net::CertificateList>();
  for (const std::unique_ptr<net::ClientCertIdentity>& identity : identities) {
    certs->push_back(identity->certificate());
  }
  // DidSelectCertificates() may be called synchronously, so run the callback on
  // a separate event loop iteration to avoid potential reentrancy bugs.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SelectCertificatesState::OnSuccess,
                                std::move(state), FROM_HERE, std::move(certs)));
}

// Filters the obtained certificates on a worker thread. Used by
// DidGetCertificates().
void FilterCertificatesOnWorkerThread(
    std::unique_ptr<GetCertificatesState> state) {
  std::unique_ptr<net::CertificateList> client_certs(new net::CertificateList);
  for (net::ScopedCERTCertificateList::const_iterator it =
           state->certs_.begin();
       it != state->certs_.end(); ++it) {
    CERTCertificate* cert_handle = it->get();
    crypto::ScopedPK11Slot cert_slot(PK11_KeyForCertExists(cert_handle,
                                                           nullptr,    // keyPtr
                                                           nullptr));  // wincx

    // Keep only user certificates, i.e. certs for which the private key is
    // present and stored in the queried slot.
    if (cert_slot != state->slot_) {
      continue;
    }

    // Allow UTF-8 inside PrintableStrings in client certificates. See
    // crbug.com/770323 and crbug.com/788655.
    net::X509Certificate::UnsafeCreateOptions options;
    options.printable_string_is_utf8 = true;
    scoped_refptr<net::X509Certificate> cert =
        net::x509_util::CreateX509CertificateFromCERTCertificate(cert_handle,
                                                                 {}, options);
    if (!cert) {
      continue;
    }

    client_certs->push_back(std::move(cert));
  }

  state->OnSuccess(FROM_HERE, std::move(client_certs));
}

// Passes the obtained certificates to the worker thread for filtering. Used by
// GetCertificatesWithDB().
void DidGetCertificates(std::unique_ptr<GetCertificatesState> state,
                        net::ScopedCERTCertificateList all_certs) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  state->certs_ = std::move(all_certs);
  // This task interacts with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FilterCertificatesOnWorkerThread, std::move(state)));
}

// Continues getting certificates with the obtained NSSCertDatabase. Used by
// GetCertificates().
void GetCertificatesWithDB(std::unique_ptr<GetCertificatesState> state,
                           net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Get the pointer to slot before transferring ownership of |state| to the
  // callback's bound arguments.
  PK11SlotInfo* slot = state->slot_.get();
  cert_db->ListCertsInSlot(
      base::BindOnce(&DidGetCertificates, std::move(state)), slot);
}

// Returns true if |public_key| is relevant as a "platform key" that should be
// visible to chrome extensions / subsystems.
bool ShouldIncludePublicKey(SECKEYPublicKey* public_key) {
  crypto::ScopedSECItem cka_id(SECITEM_AllocItem(/*arena=*/nullptr,
                                                 /*item=*/nullptr,
                                                 /*len=*/0));
  if (PK11_ReadRawAttribute(
          /*objType=*/PK11_TypePubKey, public_key, CKA_ID, cka_id.get()) !=
      SECSuccess) {
    return false;
  }

  std::string_view cka_id_str(reinterpret_cast<char*>(cka_id->data),
                              cka_id->len);

  // Only keys generated/stored by extensions/Chrome should be visible to
  // extensions. Oemcrypto stores its key in the TPM, but that should not
  // be exposed. Look at exposing additional attributes or changing the slot
  // that Oemcrypto stores keys, so that it can be done based on properties
  // of the key. See https://crbug/com/1136396
  if (cka_id_str == "arc-oemcrypto") {
    VLOG(0) << "Filtered out arc-oemcrypto public key.";
    return false;
  }
  return true;
}

// Does the actual retrieval of the SubjectPublicKeyInfo string on a worker
// thread. Used by GetAllKeysWithDb().
void GetAllKeysOnWorkerThread(std::unique_ptr<GetAllKeysState> state) {
  DCHECK(state->slot_.get());

  std::vector<std::vector<uint8_t>> public_key_spki_der_list;

  // This assumes that there might be a public key on a slot that
  // does not have a corresponding private key. The key is then considered
  // partially deleted and should be treated as deleted (it eventually will be).
  crypto::ScopedSECKEYPublicKeyList public_keys(
      PK11_ListPublicKeysInSlot(state->slot_.get(), /*nickname=*/nullptr));

  if (!public_keys) {
    state->OnSuccess(FROM_HERE, std::move(public_key_spki_der_list));
    return;
  }

  for (SECKEYPublicKeyListNode* node = PUBKEY_LIST_HEAD(public_keys);
       !PUBKEY_LIST_END(node, public_keys); node = PUBKEY_LIST_NEXT(node)) {
    if (!ShouldIncludePublicKey(node->key)) {
      continue;
    }

    crypto::ScopedSECItem subject_public_key_info(
        SECKEY_EncodeDERSubjectPublicKeyInfo(node->key));
    if (!subject_public_key_info) {
      LOG(WARNING) << "Could not encode subject public key info.";
      continue;
    }
    if (subject_public_key_info->len == 0) {
      continue;
    }
    const std::vector<uint8_t> pubkey =
        ScopedSECItemToBytes(subject_public_key_info);
    crypto::ScopedSECKEYPrivateKey rsa_key =
        crypto::FindNSSKeyFromPublicKeyInfoInSlot(pubkey, state->slot_.get());
    if (rsa_key) {
      public_key_spki_der_list.push_back(pubkey);
    }
  }

  state->OnSuccess(FROM_HERE, std::move(public_key_spki_der_list));
}

// Continues the retrieval of the SubjectPublicKeyInfo list with |cert_db|.
// Used by GetAllKeys().
void GetAllKeysWithDb(std::unique_ptr<GetAllKeysState> state,
                      net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetAllKeysOnWorkerThread, std::move(state)));
}

// Does the actual certificate importing on the IO thread. Used by
// ImportCertificate().
void ImportCertificateWithDB(std::unique_ptr<ImportCertificateState> state,
                             net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!state->certificate_) {
    state->OnError(FROM_HERE, Status::kNetErrorCertificateInvalid);
    return;
  }
  if (state->certificate_->HasExpired()) {
    state->OnError(FROM_HERE, Status::kNetErrorCertificateDateInvalid);
    return;
  }

  net::ScopedCERTCertificate nss_cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(
          state->certificate_.get());
  if (!nss_cert) {
    state->OnError(FROM_HERE, Status::kNetErrorCertificateInvalid);
    return;
  }

  // Check that the private key is in the correct slot.
  crypto::ScopedPK11Slot slot(
      PK11_KeyForCertExists(nss_cert.get(), nullptr, nullptr));
  if (slot.get() != state->slot_.get()) {
    state->OnError(FROM_HERE, Status::kErrorKeyNotFound);
    return;
  }

  const net::Error import_status =
      static_cast<net::Error>(cert_db->ImportUserCert(nss_cert.get()));
  if (import_status != net::OK) {
    LOG(ERROR) << "Could not import certificate: "
               << net::ErrorToString(import_status);
    state->OnError(FROM_HERE, Status::kNetErrorAddUserCertFailed);
    return;
  }

  state->OnSuccess(FROM_HERE);
}

// Called on IO thread after the certificate removal is finished.
void DidRemoveCertificate(std::unique_ptr<RemoveCertificateState> state,
                          bool certificate_found,
                          bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // CertificateNotFound error has precedence over an internal error.
  if (!certificate_found) {
    state->OnError(FROM_HERE, Status::kErrorCertificateNotFound);
    return;
  }
  if (!success) {
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  state->OnSuccess(FROM_HERE);
}

// Does the actual certificate removal on the IO thread. Used by
// RemoveCertificate().
void RemoveCertificateWithDB(std::unique_ptr<RemoveCertificateState> state,
                             net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  PRBool certificate_found;
  net::ScopedCERTCertificate nss_cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(
          state->certificate_.get());
  if (!nss_cert ||
      CERT_GetCertIsPerm(nss_cert.get(), &certificate_found) != SECSuccess) {
    state->OnError(FROM_HERE, Status::kNetErrorCertificateInvalid);
    return;
  }

  cert_db->DeleteCertAndKeyAsync(
      std::move(nss_cert),
      base::BindOnce(&DidRemoveCertificate, std::move(state),
                     certificate_found != PR_FALSE));
}

// Does the actual key pair removal on a worker thread. Used by
// RemoveKeyWithDb().
void RemoveKeyOnWorkerThread(std::unique_ptr<RemoveKeyState> state) {
  DCHECK(state->slot_.get());

  crypto::ScopedSECKEYPrivateKey private_key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_.get());

  if (!private_key) {
    state->OnError(FROM_HERE, Status::kErrorKeyNotFound);
    return;
  }

  crypto::ScopedSECKEYPublicKey public_key(
      SECKEY_ConvertToPublicKey(private_key.get()));

  // PK11_DeleteTokenPrivateKey function frees the privKey structure
  // unconditionally, and thus releasing the ownership of the passed private
  // key.
  // |force| is set to false so as not to delete the key if there are any
  // matching certificates.
  if (PK11_DeleteTokenPrivateKey(/*privKey=*/private_key.release(),
                                 /*force=*/false) != SECSuccess) {
    LOG(ERROR) << "Cannot delete private key";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  // PK11_DeleteTokenPublicKey function frees the pubKey structure
  // unconditionally, and thus releasing the ownership of the passed private
  // key.
  if (PK11_DeleteTokenPublicKey(/*pubKey=*/public_key.release()) !=
      SECSuccess) {
    LOG(WARNING) << "Cannot delete public key";
  }

  state->OnSuccess(FROM_HERE);
}

// Continues removing the key pair with the obtained |cert_db|. Called by
// RemoveKey().
void RemoveKeyWithDb(std::unique_ptr<RemoveKeyState> state,
                     net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&RemoveKeyOnWorkerThread, std::move(state)));
}

// Does the actual symmetric key removal on a worker thread. Used by
// RemoveSymKeyWithDb().
void RemoveSymKeyOnWorkerThread(std::unique_ptr<RemoveSymKeyState> state) {
  if (!state->slot_) {
    LOG(ERROR) << "No slot.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  crypto::ScopedPK11SymKey key =
      GetSymKey(state->key_id_, state->slot_.get(), std::nullopt);
  if (!key) {
    LOG(ERROR) << "Couldn't find the symmetric key.";
    state->OnError(FROM_HERE, Status::kErrorKeyNotFound);
    return;
  }

  if (PK11_DeleteTokenSymKey(key.get()) != SECSuccess) {
    LOG(ERROR) << "Failed to delete key.";
    state->OnError(FROM_HERE, Status::kErrorInternal);
    return;
  }

  state->OnSuccess(FROM_HERE);
}

// Continues removing the symmetric key with the obtained |cert_db|. Called by
// RemoveSymKey().
void RemoveSymKeyWithDb(std::unique_ptr<RemoveSymKeyState> state,
                        net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&RemoveSymKeyOnWorkerThread, std::move(state)));
}

// Does the actual work to determine which tokens are available.
void GetTokensWithDB(std::unique_ptr<GetTokensState> state,
                     net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<TokenId> token_ids;

  // The user token will be unavailable in case of no logged in user in this
  // profile.
  if (cert_db->GetPrivateSlot()) {
    token_ids.push_back(TokenId::kUser);
  }

  if (cert_db->GetSystemSlot()) {
    token_ids.push_back(TokenId::kSystem);
  }

  DCHECK(!token_ids.empty());

  state->OnSuccess(FROM_HERE, std::move(token_ids));
}

// Does the actual work to determine which key is on which token.
void GetKeyLocationsWithDB(std::unique_ptr<GetKeyLocationsState> state,
                           net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<TokenId> token_ids;

  const uint8_t* public_key_uint8 =
      reinterpret_cast<const uint8_t*>(state->public_key_spki_der_.data());
  std::vector<uint8_t> public_key_vector(
      public_key_uint8, public_key_uint8 + state->public_key_spki_der_.size());

  if (cert_db->GetPrivateSlot().get()) {
    crypto::ScopedSECKEYPrivateKey rsa_key =
        crypto::FindNSSKeyFromPublicKeyInfoInSlot(
            public_key_vector, cert_db->GetPrivateSlot().get());
    if (rsa_key) {
      token_ids.push_back(TokenId::kUser);
    }
  }

  // The "system" NSSCertDatabaseChromeOS instance reuses its "system slot" as
  // "public slot", but that doesn't mean it's a user-specific slot.
  if (token_ids.empty() && cert_db->GetPublicSlot().get() &&
      cert_db->GetPublicSlot().get() != cert_db->GetSystemSlot().get()) {
    crypto::ScopedSECKEYPrivateKey rsa_key =
        crypto::FindNSSKeyFromPublicKeyInfoInSlot(
            public_key_vector, cert_db->GetPublicSlot().get());
    if (rsa_key) {
      token_ids.push_back(TokenId::kUser);
    }
  }

  if (cert_db->GetSystemSlot().get()) {
    crypto::ScopedSECKEYPrivateKey rsa_key =
        crypto::FindNSSKeyFromPublicKeyInfoInSlot(
            public_key_vector, cert_db->GetSystemSlot().get());
    if (rsa_key) {
      token_ids.push_back(TokenId::kSystem);
    }
  }

  state->OnSuccess(FROM_HERE, std::move(token_ids));
}

// Translates |type| to one of the NSS softoken module's predefined key
// attributes which are used in tests.
CK_ATTRIBUTE_TYPE TranslateKeyAttributeTypeForSoftoken(KeyAttributeType type) {
  switch (type) {
    case KeyAttributeType::kCertificateProvisioningId:
      return CKA_START_DATE;
    case KeyAttributeType::kKeyPermissions:
      return CKA_END_DATE;
  }
}

// If |map_to_softoken_attrs| is true, translates |type| to one of the softoken
// module predefined key attributes. Otherwise, applies normal translation.
CK_ATTRIBUTE_TYPE TranslateKeyAttributeType(KeyAttributeType type,
                                            bool map_to_softoken_attrs) {
  if (map_to_softoken_attrs) {
    return TranslateKeyAttributeTypeForSoftoken(type);
  }

  switch (type) {
    case KeyAttributeType::kCertificateProvisioningId:
      return pkcs11_custom_attributes::kCkaChromeOsBuiltinProvisioningProfileId;
    case KeyAttributeType::kKeyPermissions:
      return pkcs11_custom_attributes::kCkaChromeOsKeyPermissions;
  }
}

// Does the actual attribute value setting. Called by
// SetAttributeForKeyWithDb().
void SetAttributeForKeyWithDbOnWorkerThread(
    std::unique_ptr<SetAttributeForKeyState> state) {
  DCHECK(state->slot_.get());

  crypto::ScopedSECKEYPrivateKey private_key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_.get());
  if (!private_key) {
    state->OnError(FROM_HERE, Status::kErrorKeyNotFound);
    return;
  }

  // This SECItem will point to data owned by |state| so it is not necessary to
  // use ScopedSECItem.
  SECItem attribute_value;
  attribute_value.data = const_cast<uint8_t*>(state->attribute_value_.data());
  attribute_value.len = state->attribute_value_.size();
  if (PK11_WriteRawAttribute(
          /*objType=*/PK11_TypePrivKey, private_key.get(),
          state->attribute_type_, &attribute_value) != SECSuccess) {
    state->OnError(FROM_HERE, Status::kErrorKeyAttributeSettingFailed);
    return;
  }

  state->OnSuccess(FROM_HERE);
}

// Continues setting the attribute with the obtained NSSCertDatabase.
// Called by SetAttributeForKey().
void SetAttributeForKeyWithDb(std::unique_ptr<SetAttributeForKeyState> state,
                              net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task could interact with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SetAttributeForKeyWithDbOnWorkerThread,
                     std::move(state)));
}

// Does the actual attribute value retrieval. Called by
// GetAttributeForKeyWithDb().
void GetAttributeForKeyWithDbOnWorkerThread(
    std::unique_ptr<GetAttributeForKeyState> state) {
  DCHECK(state->slot_.get());

  crypto::ScopedSECKEYPrivateKey private_key =
      GetPrivateKey(state->public_key_spki_der_, state->slot_.get());

  if (!private_key) {
    state->OnError(FROM_HERE, Status::kErrorKeyNotFound);
    return;
  }

  crypto::ScopedSECItem attribute_value(SECITEM_AllocItem(/*arena=*/nullptr,
                                                          /*item=*/nullptr,
                                                          /*len=*/0));
  DCHECK(attribute_value.get());

  if (PK11_ReadRawAttribute(
          /*objType=*/PK11_TypePrivKey, private_key.get(),
          state->attribute_type_, attribute_value.get()) != SECSuccess) {
    // CKR_ATTRIBUTE_TYPE_INVALID is a cryptoki function return value which is
    // returned by Chaps if the attribute was not set before for the key. NSS
    // maps this error to SEC_ERROR_BAD_DATA. This error is captured here so as
    // not to return an |error| in cases of retrieving unset key attributes and
    // to return nullopt |attribute_value| instead.
    int error = PORT_GetError();
    if (error == SEC_ERROR_BAD_DATA) {
      state->OnSuccess(FROM_HERE, /*attribute_value=*/std::nullopt);
      return;
    }

    state->OnError(FROM_HERE, Status::kErrorKeyAttributeRetrievalFailed);
    return;
  }

  std::string attribute_value_str;
  if (attribute_value->len > 0) {
    attribute_value_str.assign(attribute_value->data,
                               attribute_value->data + attribute_value->len);
  }

  state->OnSuccess(FROM_HERE, ScopedSECItemToBytes(attribute_value));
}

// Continues retrieving the attribute with the obtained NSSCertDatabase.
// Called by GetAttributeForKey().
void GetAttributeForKeyWithDb(std::unique_ptr<GetAttributeForKeyState> state,
                              net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task could interact with the TPM, hence MayBlock().
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetAttributeForKeyWithDbOnWorkerThread,
                     std::move(state)));
}

void IsKeyOnTokenWithDb(std::unique_ptr<IsKeyOnTokenState> state,
                        net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(state->slot_.get());

  bool key_on_slot =
      GetPrivateKey(state->public_key_spki_der_, state->slot_.get()) != nullptr;
  state->OnSuccess(FROM_HERE, key_on_slot);
}

}  // namespace

void PlatformKeysServiceImpl::GenerateSymKey(TokenId token_id,
                                             std::vector<uint8_t> key_id,
                                             int key_size,
                                             SymKeyType key_type,
                                             GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<GenerateSymKeyState>(
      weak_factory_.GetWeakPtr(), std::move(key_id), key_size, key_type,
      std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::BindOnce(&GenerateSymKeyWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::GenerateRSAKey(TokenId token_id,
                                             unsigned int modulus_length_bits,
                                             bool sw_backed,
                                             GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<GenerateRSAKeyState>(
      weak_factory_.GetWeakPtr(), modulus_length_bits, sw_backed, token_id,
      std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }
  if (modulus_length_bits > kMaxRSAModulusLengthBits) {
    state->OnError(FROM_HERE, Status::kErrorAlgorithmNotSupported);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::BindOnce(&GenerateRSAKeyWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::GenerateECKey(TokenId token_id,
                                            const std::string named_curve,
                                            GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<GenerateECKeyState>(
      weak_factory_.GetWeakPtr(), std::move(named_curve), token_id,
      std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }
  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::BindOnce(&GenerateECKeyWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::EncryptDecryptAES(
    chromeos::platform_keys::TokenId token_id,
    std::vector<uint8_t>& input_data,
    std::vector<uint8_t>& key_id,
    std::string& algorithm,
    std::vector<uint8_t>& init_vector,
    EncryptDecryptCallback callback,
    OperationType operation_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<EncryptDecryptState>(
      weak_factory_.GetWeakPtr(), std::move(input_data), std::move(key_id),
      std::move(algorithm), std::move(init_vector), operation_type,
      std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative status codes and we can double check that we
  // use a key of the correct token.
  GetCertDatabase(token_id,
                  base::BindOnce(&EncryptDecryptAESWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::DecryptAES(
    chromeos::platform_keys::TokenId token_id,
    std::vector<uint8_t> encrypted_data,
    std::vector<uint8_t> key_id,
    std::string decrypt_algorithm,
    std::vector<uint8_t> init_vector,
    EncryptDecryptCallback callback) {
  EncryptDecryptAES(token_id, encrypted_data, key_id, decrypt_algorithm,
                    init_vector, std::move(callback), OperationType::kDecrypt);
}

void PlatformKeysServiceImpl::EncryptAES(
    chromeos::platform_keys::TokenId token_id,
    std::vector<uint8_t> data,
    std::vector<uint8_t> key_id,
    std::string encrypt_algorithm,
    std::vector<uint8_t> init_vector,
    EncryptDecryptCallback callback) {
  EncryptDecryptAES(token_id, data, key_id, encrypt_algorithm, init_vector,
                    std::move(callback), OperationType::kEncrypt);
}

void PlatformKeysServiceImpl::DeriveSymKey(
    chromeos::platform_keys::TokenId token_id,
    std::vector<uint8_t> base_key_id,
    std::vector<uint8_t> derived_key_id,
    std::vector<uint8_t> label,
    std::vector<uint8_t> context,
    chromeos::platform_keys::SymKeyType derived_key_type,
    DeriveKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<DeriveSymKeyState> state;
  state = std::make_unique<DeriveSymKeyState>(
      weak_factory_.GetWeakPtr(), std::move(base_key_id),
      std::move(derived_key_id), std::move(label), std::move(context),
      derived_key_type, std::move(callback),
      GetAllowAlternativeParamsForTesting());

  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative status codes and we can double check that we
  // use a key of the correct token.
  GetCertDatabase(token_id,
                  base::BindOnce(&DeriveSymKeyWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::SignRsaPkcs1(
    std::optional<TokenId> token_id,
    std::vector<uint8_t> data,
    std::vector<uint8_t> public_key_spki_der,
    HashAlgorithm hash_algorithm,
    SignCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<SignState>(
      weak_factory_.GetWeakPtr(), std::move(data),
      std::move(public_key_spki_der), hash_algorithm,
      /*key_type=*/KeyType::kRsassaPkcs1V15, std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative status codes and we can double check that we
  // use a key of the correct token.
  GetCertDatabase(token_id, base::BindOnce(&SignWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::SignRSAPKCS1Raw(
    std::optional<TokenId> token_id,
    std::vector<uint8_t> data,
    std::vector<uint8_t> public_key_spki_der,
    SignCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<SignState>(
      weak_factory_.GetWeakPtr(), std::move(data),
      std::move(public_key_spki_der), HashAlgorithm::HASH_ALGORITHM_NONE,
      /*key_type=*/KeyType::kRsassaPkcs1V15, std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative status codes and we can double check that we
  // use a key of the correct token.
  GetCertDatabase(token_id, base::BindOnce(&SignWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::SignEcdsa(
    std::optional<TokenId> token_id,
    std::vector<uint8_t> data,
    std::vector<uint8_t> public_key_spki_der,
    HashAlgorithm hash_algorithm,
    SignCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<SignState>(
      weak_factory_.GetWeakPtr(), std::move(data),
      std::move(public_key_spki_der), hash_algorithm,
      /*key_type=*/KeyType::kEcdsa, std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative status codes and we can double check that we
  // use a key of the correct token.
  GetCertDatabase(token_id, base::BindOnce(&SignWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::SignWithSymKey(std::optional<TokenId> token_id,
                                             std::vector<uint8_t> data,
                                             std::vector<uint8_t> key_id,
                                             SignCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<SignSymState>(
      weak_factory_.GetWeakPtr(), std::move(data), std::move(key_id),
      std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative status codes and we can double check that we
  // use a key of the correct token.
  GetCertDatabase(token_id, base::BindOnce(&SignSymWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::SelectClientCertificates(
    std::vector<std::string> certificate_authorities,
    SelectCertificatesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto cert_request_info = base::MakeRefCounted<net::SSLCertRequestInfo>();

  // Currently we do not pass down the requested certificate type to the net
  // layer, as it does not support filtering certificates by type. Rather, we
  // do not constrain the certificate type here, instead the caller has to apply
  // filtering afterwards.
  cert_request_info->cert_authorities = std::move(certificate_authorities);

  auto state = std::make_unique<SelectCertificatesState>(
      weak_factory_.GetWeakPtr(), cert_request_info, std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  state->cert_store_ = delegate_->CreateClientCertStore();

  // Note DidSelectCertificates() may be called synchronously.
  SelectCertificatesState* state_ptr = state.get();
  state_ptr->cert_store_->GetClientCerts(
      state_ptr->cert_request_info_,
      base::BindOnce(&DidSelectCertificates, std::move(state)));
}

void PlatformKeysServiceImpl::GetCertificates(
    TokenId token_id,
    GetCertificatesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<GetCertificatesState>(
      weak_factory_.GetWeakPtr(), std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }
  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::BindOnce(&GetCertificatesWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::GetAllKeys(TokenId token_id,
                                         GetAllKeysCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto state = std::make_unique<GetAllKeysState>(weak_factory_.GetWeakPtr(),
                                                 std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id, base::BindOnce(&GetAllKeysWithDb, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::ImportCertificate(
    TokenId token_id,
    const scoped_refptr<net::X509Certificate>& certificate,
    ImportCertificateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<ImportCertificateState>(
      weak_factory_.GetWeakPtr(), certificate, std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }
  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative status codes and we can double check that we
  // use a key of the correct token.
  GetCertDatabase(token_id,
                  base::BindOnce(&ImportCertificateWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::RemoveCertificate(
    TokenId token_id,
    const scoped_refptr<net::X509Certificate>& certificate,
    RemoveCertificateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<RemoveCertificateState>(
      weak_factory_.GetWeakPtr(), certificate, std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }
  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative status codes.
  GetCertDatabase(token_id,
                  base::BindOnce(&RemoveCertificateWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::RemoveKey(
    TokenId token_id,
    std::vector<uint8_t> public_key_spki_der,
    RemoveKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto state = std::make_unique<RemoveKeyState>(weak_factory_.GetWeakPtr(),
                                                std::move(public_key_spki_der),
                                                std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative status codes.
  GetCertDatabase(token_id, base::BindOnce(&RemoveKeyWithDb, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::RemoveSymKey(TokenId token_id,
                                           std::vector<uint8_t> key_id,
                                           RemoveKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto state = std::make_unique<RemoveSymKeyState>(
      weak_factory_.GetWeakPtr(), std::move(key_id), std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative status codes.
  GetCertDatabase(token_id,
                  base::BindOnce(&RemoveSymKeyWithDb, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::GetTokens(GetTokensCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<GetTokensState>(weak_factory_.GetWeakPtr(),
                                                std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }
  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(/*token_id=*/std::nullopt /* don't get any specific slot */,
                  base::BindOnce(&GetTokensWithDB, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::GetKeyLocations(
    std::vector<uint8_t> public_key_spki_der,
    GetKeyLocationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state = std::make_unique<GetKeyLocationsState>(
      weak_factory_.GetWeakPtr(), std::move(public_key_spki_der),
      std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }
  NSSOperationState* state_ptr = state.get();

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  GetCertDatabase(
      /*token_id=*/std::nullopt /* don't get any specific slot */,
      base::BindOnce(&GetKeyLocationsWithDB, std::move(state)), delegate_.get(),
      state_ptr);
}

void PlatformKeysServiceImpl::SetAttributeForKey(
    TokenId token_id,
    std::vector<uint8_t> public_key_spki_der,
    KeyAttributeType attribute_type,
    std::vector<uint8_t> attribute_value,
    SetAttributeForKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CK_ATTRIBUTE_TYPE ck_attribute_type = TranslateKeyAttributeType(
      attribute_type,
      /*map_to_softoken_attrs=*/IsSetMapToSoftokenAttrsForTesting());

  auto state = std::make_unique<SetAttributeForKeyState>(
      weak_factory_.GetWeakPtr(), std::move(public_key_spki_der),
      ck_attribute_type, std::move(attribute_value), std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. Only setting the state slot is
  // required.
  GetCertDatabase(token_id,
                  base::BindOnce(&SetAttributeForKeyWithDb, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::GetAttributeForKey(
    TokenId token_id,
    std::vector<uint8_t> public_key_spki_der,
    KeyAttributeType attribute_type,
    GetAttributeForKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CK_ATTRIBUTE_TYPE ck_attribute_type = TranslateKeyAttributeType(
      attribute_type,
      /*map_to_softoken_attrs=*/IsSetMapToSoftokenAttrsForTesting());

  auto state = std::make_unique<GetAttributeForKeyState>(
      weak_factory_.GetWeakPtr(), std::move(public_key_spki_der),
      ck_attribute_type, std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. Only setting the state slot is
  // required.
  GetCertDatabase(token_id,
                  base::BindOnce(&GetAttributeForKeyWithDb, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::IsKeyOnToken(
    TokenId token_id,
    std::vector<uint8_t> public_key_spki_der,
    IsKeyOnTokenCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto state = std::make_unique<IsKeyOnTokenState>(
      weak_factory_.GetWeakPtr(), std::move(public_key_spki_der),
      std::move(callback));
  if (delegate_->IsShutDown()) {
    state->OnError(FROM_HERE, Status::kErrorShutDown);
    return;
  }

  // Get the pointer to |state| before transferring ownership of |state| to the
  // callback's bound arguments.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. Only setting the state slot is
  // required.
  GetCertDatabase(token_id,
                  base::BindOnce(&IsKeyOnTokenWithDb, std::move(state)),
                  delegate_.get(), state_ptr);
}

void PlatformKeysServiceImpl::SetMapToSoftokenAttrsForTesting(
    bool map_to_softoken_attrs_for_testing) {
  map_to_softoken_attrs_for_testing_ = map_to_softoken_attrs_for_testing;
}

bool PlatformKeysServiceImpl::IsSetMapToSoftokenAttrsForTesting() {
  return map_to_softoken_attrs_for_testing_;
}

void PlatformKeysServiceImpl::SetAllowAlternativeParamsForTesting(
    bool allow_alternative_params_for_testing) {
  allow_alternative_params_for_testing_ = allow_alternative_params_for_testing;
}

bool PlatformKeysServiceImpl::GetAllowAlternativeParamsForTesting() {
  return allow_alternative_params_for_testing_;
}
}  // namespace ash::platform_keys
