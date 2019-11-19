// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cert.h>
#include <cryptohi.h>
#include <keyhi.h>
#include <secder.h>
#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/net/client_cert_store_chromeos.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/profiles/profile.h"
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
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/third_party/mozilla_security_manager/nsNSSCertificateDB.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

using content::BrowserContext;
using content::BrowserThread;

namespace {
const char kErrorInternal[] = "Internal Error.";
const char kErrorKeyNotFound[] = "Key not found.";
const char kErrorCertificateNotFound[] = "Certificate could not be found.";
const char kErrorAlgorithmNotSupported[] = "Algorithm not supported.";

// The current maximal RSA modulus length that ChromeOS's TPM supports for key
// generation.
const unsigned int kMaxRSAModulusLengthBits = 2048;
}

namespace chromeos {

namespace platform_keys {

namespace {

// Base class to store state that is common to all NSS database operations and
// to provide convenience methods to call back.
// Keeps track of the originating task runner.
class NSSOperationState {
 public:
  NSSOperationState();
  virtual ~NSSOperationState() {}

  // Called if an error occurred during the execution of the NSS operation
  // described by this object.
  virtual void OnError(const base::Location& from,
                       const std::string& error_message) = 0;

  crypto::ScopedPK11Slot slot_;

  // The task runner on which the NSS operation was called. Any reply must be
  // posted to this runner.
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NSSOperationState);
};

typedef base::Callback<void(net::NSSCertDatabase* cert_db)> GetCertDBCallback;

// Used by GetCertDatabaseOnIOThread and called back with the requested
// NSSCertDatabase.
// If |token_id| is not empty, sets |slot_| of |state| accordingly and calls
// |callback| if the database was successfully retrieved.
void DidGetCertDBOnIOThread(const std::string& token_id,
                            const GetCertDBCallback& callback,
                            NSSOperationState* state,
                            net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!cert_db) {
    LOG(ERROR) << "Couldn't get NSSCertDatabase.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  if (!token_id.empty()) {
    if (token_id == kTokenIdUser)
      state->slot_ = cert_db->GetPrivateSlot();
    else if (token_id == kTokenIdSystem)
      state->slot_ = cert_db->GetSystemSlot();

    if (!state->slot_) {
      LOG(ERROR) << "Slot for token id '" << token_id << "' not available.";
      state->OnError(FROM_HERE, kErrorInternal);
      return;
    }
  }

  callback.Run(cert_db);
}

// Retrieves the NSSCertDatabase from |context| and, if |token_id| is not empty,
// the slot for |token_id|.
// Must be called on the IO thread.
void GetCertDatabaseOnIOThread(const std::string& token_id,
                               const GetCertDBCallback& callback,
                               content::ResourceContext* context,
                               NSSOperationState* state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::NSSCertDatabase* cert_db = GetNSSCertDatabaseForResourceContext(
      context, base::Bind(&DidGetCertDBOnIOThread, token_id, callback, state));

  if (cert_db)
    DidGetCertDBOnIOThread(token_id, callback, state, cert_db);
}

// Asynchronously fetches the NSSCertDatabase for |browser_context| and, if
// |token_id| is not empty, the slot for |token_id|. Stores the slot in |state|
// and passes the database to |callback|. Will run |callback| on the IO thread.
void GetCertDatabase(const std::string& token_id,
                     const GetCertDBCallback& callback,
                     BrowserContext* browser_context,
                     NSSOperationState* state) {
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&GetCertDatabaseOnIOThread, token_id, callback,
                                browser_context->GetResourceContext(), state));
}

class GenerateRSAKeyState : public NSSOperationState {
 public:
  GenerateRSAKeyState(unsigned int modulus_length_bits,
                      const subtle::GenerateKeyCallback& callback);
  ~GenerateRSAKeyState() override {}

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, std::string() /* no public key */, error_message);
  }

  void CallBack(const base::Location& from,
                const std::string& public_key_spki_der,
                const std::string& error_message) {
    origin_task_runner_->PostTask(
        from, base::Bind(callback_, public_key_spki_der, error_message));
  }

  const unsigned int modulus_length_bits_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  subtle::GenerateKeyCallback callback_;
};

class SignRSAState : public NSSOperationState {
 public:
  SignRSAState(const std::string& data,
               const std::string& public_key_spki_der,
               bool sign_direct_pkcs_padded,
               HashAlgorithm hash_algorithm,
               const subtle::SignCallback& callback);
  ~SignRSAState() override {}

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, std::string() /* no signature */, error_message);
  }

  void CallBack(const base::Location& from,
                const std::string& signature,
                const std::string& error_message) {
    origin_task_runner_->PostTask(
        from, base::Bind(callback_, signature, error_message));
  }

  // The data that will be signed.
  const std::string data_;

  // Must be the DER encoding of a SubjectPublicKeyInfo.
  const std::string public_key_spki_der_;

  // If true, |data_| will not be hashed before signing. Only PKCS#1 v1.5
  // padding will be applied before signing.
  // If false, |hash_algorithm_| must be set to a value != NONE.
  const bool sign_direct_pkcs_padded_;

  // Determines the hash algorithm that is used to digest |data| before signing.
  // Ignored if |sign_direct_pkcs_padded_| is true.
  const HashAlgorithm hash_algorithm_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  subtle::SignCallback callback_;
};

class SelectCertificatesState : public NSSOperationState {
 public:
  explicit SelectCertificatesState(
      const scoped_refptr<net::SSLCertRequestInfo>& request,
      const subtle::SelectCertificatesCallback& callback);
  ~SelectCertificatesState() override {}

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, std::unique_ptr<net::CertificateList>() /* no matches */,
             error_message);
  }

  void CallBack(const base::Location& from,
                std::unique_ptr<net::CertificateList> matches,
                const std::string& error_message) {
    origin_task_runner_->PostTask(
        from, base::Bind(callback_, base::Passed(&matches), error_message));
  }

  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  std::unique_ptr<net::ClientCertStore> cert_store_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  subtle::SelectCertificatesCallback callback_;
};

class GetCertificatesState : public NSSOperationState {
 public:
  explicit GetCertificatesState(const GetCertificatesCallback& callback);
  ~GetCertificatesState() override {}

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from,
             std::unique_ptr<net::CertificateList>() /* no certificates */,
             error_message);
  }

  void CallBack(const base::Location& from,
                std::unique_ptr<net::CertificateList> certs,
                const std::string& error_message) {
    origin_task_runner_->PostTask(
        from, base::Bind(callback_, base::Passed(&certs), error_message));
  }

  net::ScopedCERTCertificateList certs_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  GetCertificatesCallback callback_;
};

class ImportCertificateState : public NSSOperationState {
 public:
  ImportCertificateState(const scoped_refptr<net::X509Certificate>& certificate,
                         const ImportCertificateCallback& callback);
  ~ImportCertificateState() override {}

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, error_message);
  }

  void CallBack(const base::Location& from, const std::string& error_message) {
    origin_task_runner_->PostTask(from, base::Bind(callback_, error_message));
  }

  scoped_refptr<net::X509Certificate> certificate_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  ImportCertificateCallback callback_;
};

class RemoveCertificateState : public NSSOperationState {
 public:
  RemoveCertificateState(const scoped_refptr<net::X509Certificate>& certificate,
                         const RemoveCertificateCallback& callback);
  ~RemoveCertificateState() override {}

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, error_message);
  }

  void CallBack(const base::Location& from, const std::string& error_message) {
    origin_task_runner_->PostTask(from, base::Bind(callback_, error_message));
  }

  scoped_refptr<net::X509Certificate> certificate_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  RemoveCertificateCallback callback_;
};

class GetTokensState : public NSSOperationState {
 public:
  explicit GetTokensState(const GetTokensCallback& callback);
  ~GetTokensState() override {}

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from,
             std::unique_ptr<std::vector<std::string>>() /* no token ids */,
             error_message);
  }

  void CallBack(const base::Location& from,
                std::unique_ptr<std::vector<std::string>> token_ids,
                const std::string& error_message) {
    origin_task_runner_->PostTask(
        from, base::Bind(callback_, base::Passed(&token_ids), error_message));
  }

 private:
  // Must be called on origin thread, therefore use CallBack().
  GetTokensCallback callback_;
};

class GetKeyLocationsState : public NSSOperationState {
 public:
  GetKeyLocationsState(const std::string& public_key_spki_der,
                       const GetKeyLocationsCallback& callback);
  ~GetKeyLocationsState() override {}

  void OnError(const base::Location& from,
               const std::string& error_message) override {
    CallBack(from, std::vector<std::string>(), error_message);
  }

  void CallBack(const base::Location& from,
                const std::vector<std::string>& token_ids,
                const std::string& error_message) {
    origin_task_runner_->PostTask(
        from, base::BindOnce(callback_, token_ids, error_message));
  }

  // Must be a DER encoding of a SubjectPublicKeyInfo.
  const std::string public_key_spki_der_;

 private:
  // Must be called on origin thread, therefore use CallBack().
  GetKeyLocationsCallback callback_;
};

NSSOperationState::NSSOperationState()
    : origin_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
}

GenerateRSAKeyState::GenerateRSAKeyState(
    unsigned int modulus_length_bits,
    const subtle::GenerateKeyCallback& callback)
    : modulus_length_bits_(modulus_length_bits), callback_(callback) {
}

SignRSAState::SignRSAState(const std::string& data,
                           const std::string& public_key_spki_der,
                           bool sign_direct_pkcs_padded,
                           HashAlgorithm hash_algorithm,
                           const subtle::SignCallback& callback)
    : data_(data),
      public_key_spki_der_(public_key_spki_der),
      sign_direct_pkcs_padded_(sign_direct_pkcs_padded),
      hash_algorithm_(hash_algorithm),
      callback_(callback) {}

SelectCertificatesState::SelectCertificatesState(
    const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
    const subtle::SelectCertificatesCallback& callback)
    : cert_request_info_(cert_request_info), callback_(callback) {}

GetCertificatesState::GetCertificatesState(
    const GetCertificatesCallback& callback)
    : callback_(callback) {
}

ImportCertificateState::ImportCertificateState(
    const scoped_refptr<net::X509Certificate>& certificate,
    const ImportCertificateCallback& callback)
    : certificate_(certificate), callback_(callback) {
}

RemoveCertificateState::RemoveCertificateState(
    const scoped_refptr<net::X509Certificate>& certificate,
    const RemoveCertificateCallback& callback)
    : certificate_(certificate), callback_(callback) {
}

GetTokensState::GetTokensState(const GetTokensCallback& callback)
    : callback_(callback) {
}

GetKeyLocationsState::GetKeyLocationsState(
    const std::string& public_key_spki_der,
    const GetKeyLocationsCallback& callback)
    : public_key_spki_der_(public_key_spki_der), callback_(callback) {}

// Does the actual key generation on a worker thread. Used by
// GenerateRSAKeyWithDB().
void GenerateRSAKeyOnWorkerThread(std::unique_ptr<GenerateRSAKeyState> state) {
  if (!state->slot_) {
    LOG(ERROR) << "No slot.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  if (!crypto::GenerateRSAKeyPairNSS(
          state->slot_.get(), state->modulus_length_bits_, true /* permanent */,
          &public_key, &private_key)) {
    LOG(ERROR) << "Couldn't create key.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key.get()));
  if (!public_key_der) {
    // TODO(pneubeck): Remove private_key and public_key from storage.
    LOG(ERROR) << "Couldn't export public key.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }
  state->CallBack(
      FROM_HERE,
      std::string(reinterpret_cast<const char*>(public_key_der->data),
                  public_key_der->len),
      std::string() /* no error */);
}

// Continues generating a RSA key with the obtained NSSCertDatabase. Used by
// GenerateRSAKey().
void GenerateRSAKeyWithDB(std::unique_ptr<GenerateRSAKeyState> state,
                          net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GenerateRSAKeyOnWorkerThread, std::move(state)));
}

// Does the actual signing on a worker thread. Used by SignRSAWithDB().
void SignRSAOnWorkerThread(std::unique_ptr<SignRSAState> state) {
  const uint8_t* public_key_uint8 =
      reinterpret_cast<const uint8_t*>(state->public_key_spki_der_.data());
  std::vector<uint8_t> public_key_vector(
      public_key_uint8, public_key_uint8 + state->public_key_spki_der_.size());

  crypto::ScopedSECKEYPrivateKey rsa_key;
  if (state->slot_) {
    rsa_key = crypto::FindNSSKeyFromPublicKeyInfoInSlot(public_key_vector,
                                                        state->slot_.get());
  } else {
    rsa_key = crypto::FindNSSKeyFromPublicKeyInfo(public_key_vector);
  }

  // Fail if the key was not found or is of the wrong type.
  if (!rsa_key || SECKEY_GetPrivateKeyType(rsa_key.get()) != rsaKey) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  }

  std::string signature_str;
  if (state->sign_direct_pkcs_padded_) {
    static_assert(
        sizeof(*state->data_.data()) == sizeof(char),
        "Can't reinterpret data if it's characters are not 8 bit large.");
    SECItem input = {siBuffer,
                     reinterpret_cast<unsigned char*>(
                         const_cast<char*>(state->data_.data())),
                     state->data_.size()};

    // Compute signature of hash.
    int signature_len = PK11_SignatureLen(rsa_key.get());
    if (signature_len <= 0) {
      state->OnError(FROM_HERE, kErrorInternal);
      return;
    }

    std::vector<unsigned char> signature(signature_len);
    SECItem signature_output = {siBuffer, signature.data(), signature.size()};
    if (PK11_Sign(rsa_key.get(), &signature_output, &input) == SECSuccess)
      signature_str.assign(signature.begin(), signature.end());
  } else {
    SECOidTag sign_alg_tag = SEC_OID_UNKNOWN;
    switch (state->hash_algorithm_) {
      case HASH_ALGORITHM_SHA1:
        sign_alg_tag = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
        break;
      case HASH_ALGORITHM_SHA256:
        sign_alg_tag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
        break;
      case HASH_ALGORITHM_SHA384:
        sign_alg_tag = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
        break;
      case HASH_ALGORITHM_SHA512:
        sign_alg_tag = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
        break;
      case HASH_ALGORITHM_NONE:
        NOTREACHED();
        break;
    }

    SECItem sign_result = {siBuffer, nullptr, 0};
    if (SEC_SignData(
            &sign_result,
            reinterpret_cast<const unsigned char*>(state->data_.data()),
            state->data_.size(), rsa_key.get(), sign_alg_tag) == SECSuccess) {
      signature_str.assign(sign_result.data,
                           sign_result.data + sign_result.len);
    }
  }

  if (signature_str.empty()) {
    LOG(ERROR) << "Couldn't sign.";
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  state->CallBack(FROM_HERE, signature_str, std::string() /* no error */);
}

// Continues signing with the obtained NSSCertDatabase. Used by Sign().
void SignRSAWithDB(std::unique_ptr<SignRSAState> state,
                   net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Only the slot and not the NSSCertDatabase is required. Ignore |cert_db|.
  // This task interacts with the TPM, hence MayBlock().
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SignRSAOnWorkerThread, std::move(state)));
}

// Called when ClientCertStoreChromeOS::GetClientCerts is done. Builds the list
// of net::CertificateList and calls back. Used by SelectCertificates().
void DidSelectCertificates(std::unique_ptr<SelectCertificatesState> state,
                           net::ClientCertIdentityList identities) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Convert the ClientCertIdentityList to a CertificateList since returning
  // ClientCertIdentities would require changing the platformKeys extension
  // api. This assumes that the necessary keys can be found later with
  // crypto::FindNSSKeyFromPublicKeyInfo.
  std::unique_ptr<net::CertificateList> certs =
      std::make_unique<net::CertificateList>();
  for (const std::unique_ptr<net::ClientCertIdentity>& identity : identities)
    certs->push_back(identity->certificate());
  // DidSelectCertificates() may be called synchronously, so run the callback on
  // a separate event loop iteration to avoid potential reentrancy bugs.
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&SelectCertificatesState::CallBack,
                                std::move(state), FROM_HERE, std::move(certs),
                                std::string() /* no error */));
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
                                                           NULL,    // keyPtr
                                                           NULL));  // wincx

    // Keep only user certificates, i.e. certs for which the private key is
    // present and stored in the queried slot.
    if (cert_slot != state->slot_)
      continue;

    // Allow UTF-8 inside PrintableStrings in client certificates. See
    // crbug.com/770323 and crbug.com/788655.
    net::X509Certificate::UnsafeCreateOptions options;
    options.printable_string_is_utf8 = true;
    scoped_refptr<net::X509Certificate> cert =
        net::x509_util::CreateX509CertificateFromCERTCertificate(cert_handle,
                                                                 {}, options);
    if (!cert)
      continue;

    client_certs->push_back(std::move(cert));
  }

  state->CallBack(FROM_HERE, std::move(client_certs),
                  std::string() /* no error */);
}

// Passes the obtained certificates to the worker thread for filtering. Used by
// GetCertificatesWithDB().
void DidGetCertificates(std::unique_ptr<GetCertificatesState> state,
                        net::ScopedCERTCertificateList all_certs) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  state->certs_ = std::move(all_certs);
  // This task interacts with the TPM, hence MayBlock().
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FilterCertificatesOnWorkerThread, std::move(state)));
}

// Continues getting certificates with the obtained NSSCertDatabase. Used by
// GetCertificates().
void GetCertificatesWithDB(std::unique_ptr<GetCertificatesState> state,
                           net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Get the pointer to slot before base::Passed releases |state|.
  PK11SlotInfo* slot = state->slot_.get();
  cert_db->ListCertsInSlot(
      base::BindOnce(&DidGetCertificates, std::move(state)), slot);
}

// Does the actual certificate importing on the IO thread. Used by
// ImportCertificate().
void ImportCertificateWithDB(std::unique_ptr<ImportCertificateState> state,
                             net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!state->certificate_) {
    state->OnError(FROM_HERE, net::ErrorToString(net::ERR_CERT_INVALID));
    return;
  }
  if (state->certificate_->HasExpired()) {
    state->OnError(FROM_HERE, net::ErrorToString(net::ERR_CERT_DATE_INVALID));
    return;
  }

  net::ScopedCERTCertificate nss_cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(
          state->certificate_.get());
  if (!nss_cert) {
    state->OnError(FROM_HERE, net::ErrorToString(net::ERR_CERT_INVALID));
    return;
  }

  // Check that the private key is in the correct slot.
  crypto::ScopedPK11Slot slot(
      PK11_KeyForCertExists(nss_cert.get(), NULL, NULL));
  if (slot.get() != state->slot_.get()) {
    state->OnError(FROM_HERE, kErrorKeyNotFound);
    return;
  }

  const net::Error import_status =
      static_cast<net::Error>(cert_db->ImportUserCert(nss_cert.get()));
  if (import_status != net::OK) {
    LOG(ERROR) << "Could not import certificate.";
    state->OnError(FROM_HERE, net::ErrorToString(import_status));
    return;
  }

  state->CallBack(FROM_HERE, std::string() /* no error */);
}

// Called on IO thread after the certificate removal is finished.
void DidRemoveCertificate(std::unique_ptr<RemoveCertificateState> state,
                          bool certificate_found,
                          bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // CertificateNotFound error has precedence over an internal error.
  if (!certificate_found) {
    state->OnError(FROM_HERE, kErrorCertificateNotFound);
    return;
  }
  if (!success) {
    state->OnError(FROM_HERE, kErrorInternal);
    return;
  }

  state->CallBack(FROM_HERE, std::string() /* no error */);
}

// Does the actual certificate removal on the IO thread. Used by
// RemoveCertificate().
void RemoveCertificateWithDB(std::unique_ptr<RemoveCertificateState> state,
                             net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::ScopedCERTCertificate nss_cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(
          state->certificate_.get());
  if (!nss_cert) {
    state->OnError(FROM_HERE, net::ErrorToString(net::ERR_CERT_INVALID));
    return;
  }

  bool certificate_found = nss_cert->isperm;
  cert_db->DeleteCertAndKeyAsync(
      std::move(nss_cert), base::Bind(&DidRemoveCertificate,
                                      base::Passed(&state), certificate_found));
}

// Does the actual work to determine which tokens are available.
void GetTokensWithDB(std::unique_ptr<GetTokensState> state,
                     net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::unique_ptr<std::vector<std::string>> token_ids(
      new std::vector<std::string>);

  // The user's token is always available.
  token_ids->push_back(kTokenIdUser);
  if (cert_db->GetSystemSlot())
    token_ids->push_back(kTokenIdSystem);

  state->CallBack(FROM_HERE, std::move(token_ids),
                  std::string() /* no error */);
}

// Does the actual work to determine which key is on which token.
void GetKeyLocationsWithDB(std::unique_ptr<GetKeyLocationsState> state,
                           net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<std::string> token_ids;

  const uint8_t* public_key_uint8 =
      reinterpret_cast<const uint8_t*>(state->public_key_spki_der_.data());
  std::vector<uint8_t> public_key_vector(
      public_key_uint8, public_key_uint8 + state->public_key_spki_der_.size());

  if (cert_db->GetPrivateSlot().get()) {
    crypto::ScopedSECKEYPrivateKey rsa_key =
        crypto::FindNSSKeyFromPublicKeyInfoInSlot(
            public_key_vector, cert_db->GetPrivateSlot().get());
    if (rsa_key)
      token_ids.push_back(kTokenIdUser);
  }
  if (token_ids.empty() && cert_db->GetPublicSlot().get()) {
    crypto::ScopedSECKEYPrivateKey rsa_key =
        crypto::FindNSSKeyFromPublicKeyInfoInSlot(
            public_key_vector, cert_db->GetPublicSlot().get());
    if (rsa_key)
      token_ids.push_back(kTokenIdUser);
  }

  if (cert_db->GetSystemSlot().get()) {
    crypto::ScopedSECKEYPrivateKey rsa_key =
        crypto::FindNSSKeyFromPublicKeyInfoInSlot(
            public_key_vector, cert_db->GetSystemSlot().get());
    if (rsa_key)
      token_ids.push_back(kTokenIdSystem);
  }

  state->CallBack(FROM_HERE, std::move(token_ids),
                  std::string() /* no error */);
}

}  // namespace

namespace subtle {

void GenerateRSAKey(const std::string& token_id,
                    unsigned int modulus_length_bits,
                    const GenerateKeyCallback& callback,
                    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<GenerateRSAKeyState> state(
      new GenerateRSAKeyState(modulus_length_bits, callback));

  if (modulus_length_bits > kMaxRSAModulusLengthBits) {
    state->OnError(FROM_HERE, kErrorAlgorithmNotSupported);
    return;
  }

  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::Bind(&GenerateRSAKeyWithDB, base::Passed(&state)),
                  browser_context,
                  state_ptr);
}

void SignRSAPKCS1Digest(const std::string& token_id,
                        const std::string& data,
                        const std::string& public_key_spki_der,
                        HashAlgorithm hash_algorithm,
                        const SignCallback& callback,
                        content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<SignRSAState> state(new SignRSAState(
      data, public_key_spki_der, false /* digest before signing */,
      hash_algorithm, callback));
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages and we can double check that
  // we use a key of the correct token.
  GetCertDatabase(token_id, base::Bind(&SignRSAWithDB, base::Passed(&state)),
                  browser_context, state_ptr);
}

void SignRSAPKCS1Raw(const std::string& token_id,
                     const std::string& data,
                     const std::string& public_key_spki_der,
                     const SignCallback& callback,
                     content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<SignRSAState> state(new SignRSAState(
      data, public_key_spki_der, true /* sign directly without hashing */,
      HASH_ALGORITHM_NONE, callback));
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages and we can double check that
  // we use a key of the correct token.
  GetCertDatabase(token_id, base::Bind(&SignRSAWithDB, base::Passed(&state)),
                  browser_context, state_ptr);
}

void SelectClientCertificates(
    const std::vector<std::string>& certificate_authorities,
    const SelectCertificatesCallback& callback,
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto cert_request_info = base::MakeRefCounted<net::SSLCertRequestInfo>();

  // Currently we do not pass down the requested certificate type to the net
  // layer, as it does not support filtering certificates by type. Rather, we
  // do not constrain the certificate type here, instead the caller has to apply
  // filtering afterwards.
  cert_request_info->cert_authorities = certificate_authorities;

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          Profile::FromBrowserContext(browser_context));

  // Use the device-wide system key slot only if the user is affiliated on the
  // device.
  const bool use_system_key_slot = user->IsAffiliated();

  auto state =
      std::make_unique<SelectCertificatesState>(cert_request_info, callback);

  state->cert_store_ = std::make_unique<ClientCertStoreChromeOS>(
      nullptr,  // no additional provider
      use_system_key_slot, user->username_hash(),
      ClientCertStoreChromeOS::PasswordDelegateFactory());

  // Note DidSelectCertificates() may be called synchronously.
  SelectCertificatesState* state_ptr = state.get();
  state_ptr->cert_store_->GetClientCerts(
      *state_ptr->cert_request_info_,
      base::BindOnce(&DidSelectCertificates, std::move(state)));
}

}  // namespace subtle

std::string GetSubjectPublicKeyInfo(
    const scoped_refptr<net::X509Certificate>& certificate) {
  base::StringPiece spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
          &spki_bytes))
    return {};
  return spki_bytes.as_string();
}

bool GetPublicKey(const scoped_refptr<net::X509Certificate>& certificate,
                  net::X509Certificate::PublicKeyType* key_type,
                  size_t* key_size_bits) {
  net::X509Certificate::PublicKeyType key_type_tmp =
      net::X509Certificate::kPublicKeyTypeUnknown;
  size_t key_size_bits_tmp = 0;
  net::X509Certificate::GetPublicKeyInfo(certificate->cert_buffer(),
                                         &key_size_bits_tmp, &key_type_tmp);

  if (key_type_tmp == net::X509Certificate::kPublicKeyTypeUnknown) {
    LOG(WARNING) << "Could not extract public key of certificate.";
    return false;
  }
  if (key_type_tmp != net::X509Certificate::kPublicKeyTypeRSA) {
    LOG(WARNING) << "Keys of other type than RSA are not supported.";
    return false;
  }

  std::string spki = GetSubjectPublicKeyInfo(certificate);
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(spki.data()), spki.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
  if (!pkey) {
    LOG(WARNING) << "Could not extract public key of certificate.";
    return false;
  }
  RSA* rsa = EVP_PKEY_get0_RSA(pkey.get());
  if (!rsa) {
    LOG(WARNING) << "Could not get RSA from PKEY.";
    return false;
  }

  const BIGNUM* public_exponent;
  RSA_get0_key(rsa, nullptr /* out_n */, &public_exponent, nullptr /* out_d */);
  if (BN_get_word(public_exponent) != 65537L) {
    LOG(ERROR) << "Rejecting RSA public exponent that is unequal 65537.";
    return false;
  }

  *key_type = key_type_tmp;
  *key_size_bits = key_size_bits_tmp;
  return true;
}

void GetCertificates(const std::string& token_id,
                     const GetCertificatesCallback& callback,
                     BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<GetCertificatesState> state(
      new GetCertificatesState(callback));
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(token_id,
                  base::Bind(&GetCertificatesWithDB, base::Passed(&state)),
                  browser_context,
                  state_ptr);
}

void ImportCertificate(const std::string& token_id,
                       const scoped_refptr<net::X509Certificate>& certificate,
                       const ImportCertificateCallback& callback,
                       BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<ImportCertificateState> state(
      new ImportCertificateState(certificate, callback));
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages and we can double check that
  // we use a key of the correct token.
  GetCertDatabase(token_id,
                  base::Bind(&ImportCertificateWithDB, base::Passed(&state)),
                  browser_context,
                  state_ptr);
}

void RemoveCertificate(const std::string& token_id,
                       const scoped_refptr<net::X509Certificate>& certificate,
                       const RemoveCertificateCallback& callback,
                       BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<RemoveCertificateState> state(
      new RemoveCertificateState(certificate, callback));
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();

  // The NSSCertDatabase object is not required. But in case it's not available
  // we would get more informative error messages.
  GetCertDatabase(token_id,
                  base::Bind(&RemoveCertificateWithDB, base::Passed(&state)),
                  browser_context,
                  state_ptr);
}

void GetTokens(const GetTokensCallback& callback,
               content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<GetTokensState> state(new GetTokensState(callback));
  // Get the pointer to |state| before base::Passed releases |state|.
  NSSOperationState* state_ptr = state.get();
  GetCertDatabase(std::string() /* don't get any specific slot */,
                  base::Bind(&GetTokensWithDB, base::Passed(&state)),
                  browser_context,
                  state_ptr);
}

void GetKeyLocations(const std::string& public_key_spki_der,
                     const GetKeyLocationsCallback& callback,
                     content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto state =
      std::make_unique<GetKeyLocationsState>(public_key_spki_der, callback);
  NSSOperationState* state_ptr = state.get();

  GetCertDatabase(
      std::string() /* don't get any specific slot - we need all slots */,
      base::BindRepeating(&GetKeyLocationsWithDB, base::Passed(&state)),
      browser_context, state_ptr);
}

}  // namespace platform_keys

}  // namespace chromeos
