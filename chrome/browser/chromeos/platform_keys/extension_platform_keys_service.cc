// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom-shared.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/x509_certificate.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/crosapi/keystore_service_factory_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

using content::BrowserThread;
using crosapi::keystore_service_util::MakeEcKeystoreSigningAlgorithm;
using crosapi::keystore_service_util::MakeRsaKeystoreSigningAlgorithm;
using crosapi::mojom::KeystoreBinaryResult;
using crosapi::mojom::KeystoreBinaryResultPtr;
using crosapi::mojom::KeystoreECDSAParams;
using crosapi::mojom::KeystoreECDSAParamsPtr;
using crosapi::mojom::KeystoreError;
using crosapi::mojom::KeystorePKCS115Params;
using crosapi::mojom::KeystorePKCS115ParamsPtr;
using crosapi::mojom::KeystoreSelectClientCertificatesResult;
using crosapi::mojom::KeystoreSelectClientCertificatesResultPtr;
using crosapi::mojom::KeystoreService;
using crosapi::mojom::KeystoreSigningAlgorithm;
using crosapi::mojom::KeystoreSigningAlgorithmPtr;
using crosapi::mojom::KeystoreSigningScheme;
using crosapi::mojom::KeystoreType;

namespace chromeos {

namespace {

// Verify the allowlisted kKeyPermissionsInLoginScreen feature behaviors.
bool IsExtensionAllowlisted(const extensions::Extension* extension) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Can be nullptr if the extension is uninstalled before the SignTask is
  // completed.
  if (!extension)
    return false;

  const extensions::Feature* key_permissions_in_login_screen =
      extensions::FeatureProvider::GetBehaviorFeature(
          extensions::behavior_feature::kKeyPermissionsInLoginScreen);

  return key_permissions_in_login_screen->IsAvailableToExtension(extension)
      .is_available();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

KeystoreType KeystoreTypeFromTokenId(platform_keys::TokenId token_id) {
  switch (token_id) {
    case platform_keys::TokenId::kUser:
      return KeystoreType::kUser;
    case platform_keys::TokenId::kSystem:
      return KeystoreType::kDevice;
  }
}

KeystoreSigningScheme GetKeystoreSigningScheme(
    platform_keys::KeyType key_type,
    platform_keys::HashAlgorithm hash_algorithm) {
  switch (key_type) {
    case platform_keys::KeyType::kRsassaPkcs1V15: {
      switch (hash_algorithm) {
        case platform_keys::HASH_ALGORITHM_NONE:
          return KeystoreSigningScheme::kRsassaPkcs1V15None;
        case platform_keys::HASH_ALGORITHM_SHA1:
          return KeystoreSigningScheme::kRsassaPkcs1V15Sha1;
        case platform_keys::HASH_ALGORITHM_SHA256:
          return KeystoreSigningScheme::kRsassaPkcs1V15Sha256;
        case platform_keys::HASH_ALGORITHM_SHA384:
          return KeystoreSigningScheme::kRsassaPkcs1V15Sha384;
        case platform_keys::HASH_ALGORITHM_SHA512:
          return KeystoreSigningScheme::kRsassaPkcs1V15Sha512;
      }
    }
    case platform_keys::KeyType::kEcdsa: {
      switch (hash_algorithm) {
        case platform_keys::HASH_ALGORITHM_NONE:
          // This combination is not supported.
          return KeystoreSigningScheme::kUnknown;
        case platform_keys::HASH_ALGORITHM_SHA1:
          return KeystoreSigningScheme::kEcdsaSha1;
        case platform_keys::HASH_ALGORITHM_SHA256:
          return KeystoreSigningScheme::kEcdsaSha256;
        case platform_keys::HASH_ALGORITHM_SHA384:
          return KeystoreSigningScheme::kEcdsaSha384;
        case platform_keys::HASH_ALGORITHM_SHA512:
          return KeystoreSigningScheme::kEcdsaSha512;
      }
    }
  }
  NOTREACHED_IN_MIGRATION();
  return KeystoreSigningScheme::kUnknown;
}

// Returns appropriate KeystoreService for |browser_context|.
//
// For Lacros-Chrome it returns a remote mojo implementation owned by
// LacrosService (that is created before the start of the main loop and should
// outlive ExtensionPlatformKeysService).
//
// For Ash-Chrome the factory can return:
// * an instance owned by CrosapiManager (that is created before profiles and
// should outlive ExtensionPlatformKeysService)
// * or an appropriate keyed service that will always exist
// during ExtensionPlatformKeysService lifetime (because of KeyedService
// dependencies).
crosapi::mojom::KeystoreService* GetKeystoreService(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/191958380): Lift the restriction when *.platformKeys.* APIs are
  // implemented for secondary profiles in Lacros.
  CHECK(Profile::FromBrowserContext(browser_context)->IsMainProfile())
      << "Attempted to use an incorrect profile. Please file a bug at "
         "https://bugs.chromium.org/ if this happens.";

  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::KeystoreService>()) {
    return nullptr;
  }
  return service->GetRemote<crosapi::mojom::KeystoreService>().get();
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::KeystoreServiceFactoryAsh::GetForBrowserContext(
      browser_context);
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace

class ExtensionPlatformKeysService::Task {
 public:
  Task() = default;
  Task(const Task&) = delete;
  auto operator=(const Task&) = delete;
  virtual ~Task() = default;
  virtual void Start() = 0;
  virtual bool IsDone() = 0;
};

class ExtensionPlatformKeysService::GenerateKeyTask : public Task {
 public:
  enum class Step {
    GENERATE_KEY,
    GET_EXTENSION_PERMISSIONS,
    UPDATE_PERMISSIONS_AND_CALLBACK,
    DONE,
  };

  GenerateKeyTask(platform_keys::TokenId token_id,
                  extensions::ExtensionId extension_id,
                  GenerateKeyCallback callback,
                  ExtensionPlatformKeysService* service)
      : token_id_(token_id),
        extension_id_(std::move(extension_id)),
        callback_(std::move(callback)),
        service_(service) {}

  GenerateKeyTask(const GenerateKeyTask&) = delete;
  auto operator=(const GenerateKeyTask&) = delete;

  ~GenerateKeyTask() override = default;

  void Start() override {
    CHECK(next_step_ == Step::GENERATE_KEY);
    DoStep();
  }

  bool IsDone() override { return next_step_ == Step::DONE; }

 protected:
  virtual void GenerateKey(KeystoreService::GenerateKeyCallback callback) = 0;

  platform_keys::TokenId token_id_;
  std::vector<uint8_t> public_key_spki_der_;
  const extensions::ExtensionId extension_id_;
  GenerateKeyCallback callback_;
  std::unique_ptr<platform_keys::ExtensionKeyPermissionsService>
      extension_key_permissions_service_;
  const raw_ptr<ExtensionPlatformKeysService> service_;

 private:
  void DoStep() {
    switch (next_step_) {
      case Step::GENERATE_KEY:
        next_step_ = Step::GET_EXTENSION_PERMISSIONS;
        GenerateKey(base::BindOnce(&GenerateKeyTask::GeneratedKey,
                                   weak_factory_.GetWeakPtr()));
        return;
      case Step::GET_EXTENSION_PERMISSIONS:
        next_step_ = Step::UPDATE_PERMISSIONS_AND_CALLBACK;
        GetExtensionPermissions();
        return;
      case Step::UPDATE_PERMISSIONS_AND_CALLBACK:
        next_step_ = Step::DONE;
        UpdatePermissionsAndCallBack();
        return;
      case Step::DONE:
        service_->TaskFinished(this);
        // |this| might be invalid now.
        return;
    }
  }

  // Stores the generated key or in case of an error calls |callback_| with the
  // error status.
  void GeneratedKey(KeystoreBinaryResultPtr result) {
    using Tag = KeystoreBinaryResult::Tag;
    switch (result->which()) {
      case Tag::kError:
        next_step_ = Step::DONE;
        std::move(callback_).Run(std::vector<uint8_t>() /* no public key */,
                                 result->get_error());
        break;
      case Tag::kBlob:
        public_key_spki_der_ = std::move(result->get_blob());
        break;
    }
    DoStep();
  }

  // Gets the permissions for the extension with id |extension_id|.
  void GetExtensionPermissions() {
    platform_keys::ExtensionKeyPermissionsServiceFactory::
        GetForBrowserContextAndExtension(
            base::BindOnce(&GenerateKeyTask::GotPermissions,
                           weak_factory_.GetWeakPtr()),
            service_->browser_context_, extension_id_);
  }

  void GotPermissions(
      std::unique_ptr<platform_keys::ExtensionKeyPermissionsService>
          extension_key_permissions_service) {
    extension_key_permissions_service_ =
        std::move(extension_key_permissions_service);
    DoStep();
  }

  void UpdatePermissionsAndCallBack() {
    extension_key_permissions_service_->RegisterKeyForCorporateUsage(
        public_key_spki_der_,
        base::BindOnce(&GenerateKeyTask::OnKeyRegisteredForCorporateUsage,
                       weak_factory_.GetWeakPtr()));
  }

  void OnKeyRegisteredForCorporateUsage(bool is_error,
                                        crosapi::mojom::KeystoreError error) {
    if (!is_error) {
      std::move(callback_).Run(std::move(public_key_spki_der_),
                               /*error=*/std::nullopt);
      DoStep();
      return;
    }

    LOG(ERROR) << "Corporate key registration failed: "
               << platform_keys::KeystoreErrorToString(error);

    service_->keystore_service_->RemoveKey(
        KeystoreTypeFromTokenId(token_id_), std::move(public_key_spki_der_),
        base::BindOnce(&GenerateKeyTask::RemoveKeyCallback,
                       weak_factory_.GetWeakPtr(),
                       /*corporate_key_registration_error_status=*/error));
  }

  void RemoveKeyCallback(
      crosapi::mojom::KeystoreError corporate_key_registration_error,
      bool is_remove_error,
      KeystoreError remove_error) {
    if (is_remove_error) {
      LOG(ERROR)
          << "Failed to remove a dangling key with error: "
          << platform_keys::KeystoreErrorToString(remove_error)
          << ", after failing to register key for corporate usage with error: "
          << platform_keys::KeystoreErrorToString(
                 corporate_key_registration_error);
    }

    next_step_ = Step::DONE;
    std::move(callback_).Run(std::vector<uint8_t>() /* no public key */,
                             corporate_key_registration_error);
    DoStep();
  }

  Step next_step_ = Step::GENERATE_KEY;

  base::WeakPtrFactory<GenerateKeyTask> weak_factory_{this};
};

class ExtensionPlatformKeysService::GenerateRSAKeyTask
    : public GenerateKeyTask {
 public:
  // This key task generates an RSA key with the parameters |token_id| and
  // |modulus_length| and registers it for the extension with id |extension_id|.
  // The generated key will be passed to |callback|.
  GenerateRSAKeyTask(platform_keys::TokenId token_id,
                     unsigned int modulus_length,
                     bool sw_backed,
                     const std::string& extension_id,
                     GenerateKeyCallback callback,
                     ExtensionPlatformKeysService* service)
      : GenerateKeyTask(token_id, extension_id, std::move(callback), service),
        modulus_length_(modulus_length),
        sw_backed_(sw_backed) {}

  ~GenerateRSAKeyTask() override = default;

 private:
  // Generates the RSA key.
  void GenerateKey(KeystoreService::GenerateKeyCallback callback) override {
    service_->keystore_service_->GenerateKey(
        KeystoreTypeFromTokenId(token_id_),
        MakeRsaKeystoreSigningAlgorithm(modulus_length_, sw_backed_),
        std::move(callback));
  }

  const unsigned int modulus_length_;
  const bool sw_backed_;
};

class ExtensionPlatformKeysService::GenerateECKeyTask : public GenerateKeyTask {
 public:
  // This Task generates an EC key with the parameters |token_id| and
  // |named_curve| and registers it for the extension with id |extension_id|.
  // The generated key will be passed to |callback|.
  GenerateECKeyTask(platform_keys::TokenId token_id,
                    std::string named_curve,
                    std::string extension_id,
                    GenerateKeyCallback callback,
                    ExtensionPlatformKeysService* service)
      : GenerateKeyTask(token_id,
                        std::move(extension_id),
                        std::move(callback),
                        service),
        named_curve_(std::move(named_curve)) {}

  ~GenerateECKeyTask() override = default;

 private:
  // Generates the EC key.
  void GenerateKey(KeystoreService::GenerateKeyCallback callback) override {
    service_->keystore_service_->GenerateKey(
        KeystoreTypeFromTokenId(token_id_),
        MakeEcKeystoreSigningAlgorithm(named_curve_), std::move(callback));
  }

  const std::string named_curve_;
};

class ExtensionPlatformKeysService::SignTask : public Task {
 public:
  enum class Step {
    GET_EXTENSION_PERMISSIONS,
    CHECK_SIGN_PERMISSIONS,
    UPDATE_SIGN_PERMISSIONS,
    SIGN,
    DONE,
  };

  // This Task will check the permissions of the extension with |extension_id|
  // for the key of type |key_type| identified by |public_key_spki_der|. If the
  // permission check was positive, signs |data| with the key and passes the
  // signature to |callback|. If the extension is not allowed to use the key
  // multiple times, also updates the permission to prevent any future signing
  // operation of that extension using that same key. If an error occurs, an
  // error status is passed to |callback|.
  SignTask(std::optional<platform_keys::TokenId> token_id,
           std::vector<uint8_t> data,
           std::vector<uint8_t> public_key_spki_der,
           platform_keys::KeyType key_type,
           platform_keys::HashAlgorithm hash_algorithm,
           std::string extension_id,
           SignCallback callback,
           ExtensionPlatformKeysService* service)
      : token_id_(token_id),
        data_(std::move(data)),
        public_key_spki_der_(std::move(public_key_spki_der)),
        extension_id_(std::move(extension_id)),
        callback_(std::move(callback)),
        service_(service) {
    signing_scheme_ = GetKeystoreSigningScheme(key_type, hash_algorithm);
    DCHECK(signing_scheme_ != KeystoreSigningScheme::kUnknown);
  }

  SignTask(const SignTask&) = delete;
  auto operator=(const SignTask&) = delete;

  ~SignTask() override = default;

  void Start() override {
    CHECK(next_step_ == Step::GET_EXTENSION_PERMISSIONS);
    DoStep();
  }

  bool IsDone() override { return next_step_ == Step::DONE; }

 private:
  void DoStep() {
    switch (next_step_) {
      case Step::GET_EXTENSION_PERMISSIONS:
        next_step_ = Step::CHECK_SIGN_PERMISSIONS;
        GetExtensionPermissions();
        return;
      case Step::CHECK_SIGN_PERMISSIONS:
        next_step_ = Step::UPDATE_SIGN_PERMISSIONS;
        CheckSignPermissions();
        return;
      case Step::UPDATE_SIGN_PERMISSIONS:
        next_step_ = Step::SIGN;
        UpdateSignPermissions();
        return;
      case Step::SIGN:
        next_step_ = Step::DONE;
        Sign();
        return;
      case Step::DONE:
        service_->TaskFinished(this);
        // |this| might be invalid now.
        return;
    }
  }

  void GetExtensionPermissions() {
    platform_keys::ExtensionKeyPermissionsServiceFactory::
        GetForBrowserContextAndExtension(
            base::BindOnce(&SignTask::GotPermissions,
                           weak_factory_.GetWeakPtr()),
            service_->browser_context_, extension_id_);
  }

  void GotPermissions(
      std::unique_ptr<platform_keys::ExtensionKeyPermissionsService>
          extension_key_permissions_service) {
    extension_key_permissions_service_ =
        std::move(extension_key_permissions_service);
    DoStep();
  }

  void CheckSignPermissions() {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(service_->browser_context_)
            ->enabled_extensions()
            .GetByID(extension_id_);
    if (service_->IsUsingSigninProfile() && IsExtensionAllowlisted(extension)) {
      DoStep();
      return;
    }

    extension_key_permissions_service_->CanUseKeyForSigning(
        public_key_spki_der_,
        base::BindOnce(&SignTask::OnCanUseKeyForSigningKnown,
                       weak_factory_.GetWeakPtr()));
  }

  void OnCanUseKeyForSigningKnown(bool allowed) {
    if (!allowed) {
      std::move(callback_).Run(std::vector<uint8_t>() /* no signature */,
                               KeystoreError::kKeyNotAllowedForSigning);
      next_step_ = Step::DONE;
      DoStep();
      return;
    }

    DoStep();
  }

  // Updates the signing permissions for |public_key_spki_der_|.
  void UpdateSignPermissions() {
    extension_key_permissions_service_->SetKeyUsedForSigning(
        public_key_spki_der_,
        base::BindOnce(&SignTask::OnSetKeyUsedForSigningDone,
                       weak_factory_.GetWeakPtr()));
  }

  void OnSetKeyUsedForSigningDone(bool is_error,
                                  crosapi::mojom::KeystoreError error) {
    if (is_error) {
      LOG(ERROR) << "Marking a key used for signing failed: "
                 << platform_keys::KeystoreErrorToString(error);
      next_step_ = Step::DONE;
      std::move(callback_).Run(std::vector<uint8_t>() /* no signature */,
                               error);
      DoStep();
      return;
    }

    DoStep();
  }

  // Starts the actual signing operation and afterwards passes the signature (or
  // error) to |callback_|.
  void Sign() {
    // TODO(crbug.com/40489779): This can be simplified when mojo supports
    // optional enums.
    bool is_keystore_provided = false;
    KeystoreType keystore = KeystoreType::kUser;
    if (token_id_.has_value()) {
      is_keystore_provided = true;
      keystore = KeystoreTypeFromTokenId(token_id_.value());
    }

    service_->keystore_service_->Sign(
        is_keystore_provided, keystore, public_key_spki_der_, signing_scheme_,
        data_, base::BindOnce(&SignTask::DidSign, weak_factory_.GetWeakPtr()));
  }

  void DidSign(KeystoreBinaryResultPtr result) {
    switch (result->which()) {
      case KeystoreBinaryResult::Tag::kError:
        std::move(callback_).Run(/*signature=*/std::vector<uint8_t>(),
                                 result->get_error());
        break;
      case KeystoreBinaryResult::Tag::kBlob:
        std::move(callback_).Run(
            /*signature=*/std::move(result->get_blob()),
            /*error=*/std::nullopt);
        break;
    }
    DoStep();
  }

  Step next_step_ = Step::GET_EXTENSION_PERMISSIONS;

  std::optional<platform_keys::TokenId> token_id_;
  const std::vector<uint8_t> data_;
  const std::vector<uint8_t> public_key_spki_der_;

  KeystoreSigningScheme signing_scheme_;
  const extensions::ExtensionId extension_id_;
  SignCallback callback_;
  std::unique_ptr<platform_keys::ExtensionKeyPermissionsService>
      extension_key_permissions_service_;
  const raw_ptr<ExtensionPlatformKeysService> service_;
  base::WeakPtrFactory<SignTask> weak_factory_{this};
};

class ExtensionPlatformKeysService::SelectTask : public Task {
 public:
  enum class Step {
    GET_EXTENSION_PERMISSIONS,
    GET_MATCHING_CERTS,
    CHECK_KEY_PERMISSIONS,
    INTERSECT_WITH_INPUT_CERTS,
    SELECT_CERTS,
    UPDATE_PERMISSION,
    PASS_RESULTING_CERTS,
    DONE,
  };

  // This task determines all known client certs matching |request| and that are
  // elements of |input_client_certificates|, if given. If |interactive| is
  // true, calls |service->select_delegate_->Select()| to select a cert from all
  // matches. The extension with |extension_id| will be granted unlimited sign
  // permission for the selected cert. Finally, either the selection or, if
  // |interactive| is false, matching certs that the extension has permission
  // for are passed to |callback|.
  SelectTask(const platform_keys::ClientCertificateRequest& request,
             std::unique_ptr<net::CertificateList> input_client_certificates,
             bool interactive,
             std::string extension_id,
             SelectCertificatesCallback callback,
             content::WebContents* web_contents,
             ExtensionPlatformKeysService* service)
      : request_(request),
        input_client_certificates_(std::move(input_client_certificates)),
        interactive_(interactive),
        extension_id_(std::move(extension_id)),
        callback_(std::move(callback)),
        web_contents_(web_contents),
        service_(service) {}

  SelectTask(const SelectTask&) = delete;
  auto operator=(const SelectTask) = delete;

  ~SelectTask() override = default;

  void Start() override {
    CHECK(next_step_ == Step::GET_EXTENSION_PERMISSIONS);
    DoStep();
  }

  bool IsDone() override { return next_step_ == Step::DONE; }

 private:
  void DoStep() {
    switch (next_step_) {
      case Step::GET_EXTENSION_PERMISSIONS:
        next_step_ = Step::GET_MATCHING_CERTS;
        GetExtensionPermissions();
        return;
      case Step::GET_MATCHING_CERTS:
        next_step_ = Step::CHECK_KEY_PERMISSIONS;
        GetMatchingCerts();
        return;
      case Step::CHECK_KEY_PERMISSIONS:
        // Don't advance to the next step yet - CheckKeyPermissions is repeated
        // for all matching certs. The next step will be selected in
        // CheckKeyPermissions.
        CheckKeyPermissions(Step::INTERSECT_WITH_INPUT_CERTS /* next_step */);
        return;
      case Step::INTERSECT_WITH_INPUT_CERTS:
        if (interactive_)
          next_step_ = Step::SELECT_CERTS;
        else  // Skip SelectCerts and UpdatePermission if not interactive.
          next_step_ = Step::PASS_RESULTING_CERTS;
        IntersectWithInputCerts();
        return;
      case Step::SELECT_CERTS:
        next_step_ = Step::UPDATE_PERMISSION;
        SelectCerts();
        return;
      case Step::UPDATE_PERMISSION:
        next_step_ = Step::PASS_RESULTING_CERTS;
        UpdatePermission();
        return;
      case Step::PASS_RESULTING_CERTS:
        next_step_ = Step::DONE;
        PassResultingCerts();
        return;
      case Step::DONE:
        service_->TaskFinished(this);
        // |this| might be invalid now.
        return;
    }
  }

  void GetExtensionPermissions() {
    platform_keys::ExtensionKeyPermissionsServiceFactory::
        GetForBrowserContextAndExtension(
            base::BindOnce(&SelectTask::GotPermissions,
                           weak_factory_.GetWeakPtr()),
            service_->browser_context_, extension_id_);
  }

  void GotPermissions(
      std::unique_ptr<platform_keys::ExtensionKeyPermissionsService>
          extension_key_permissions_service) {
    extension_key_permissions_service_ =
        std::move(extension_key_permissions_service);
    DoStep();
  }

  // Retrieves all certificates matching |request_|. Will call back to
  // |GotMatchingCerts()|.
  void GetMatchingCerts() {
    service_->keystore_service_->SelectClientCertificates(
        request_.certificate_authorities,
        base::BindOnce(&SelectTask::GotMatchingCerts,
                       weak_factory_.GetWeakPtr()));
  }

  // If the certificate request could be processed successfully, |matches| will
  // contain the list of matching certificates (maybe empty). If an error
  // occurred, |matches| will be null. Note that the order of |matches|, based
  // on the expiration/issuance date, is relevant and must be preserved in any
  // processing of the list.
  void GotMatchingCerts(KeystoreSelectClientCertificatesResultPtr result) {
    if (result->which() ==
        KeystoreSelectClientCertificatesResult::Tag::kError) {
      next_step_ = Step::DONE;
      std::move(callback_).Run(nullptr /* no certificates */,
                               result->get_error());
      DoStep();
      return;
    }

    for (const std::vector<uint8_t>& binary_cert : result->get_certificates()) {
      scoped_refptr<net::X509Certificate> certificate =
          net::X509Certificate::CreateFromBytes(binary_cert);

      // Filter the retrieved certificates returning only those whose type
      // is equal to one of the entries in the type field of the certificate
      // request.
      // If the type field does not contain any entries, certificates of all
      // types shall be returned.
      if (!request_.certificate_key_types.empty()) {
        net::X509Certificate::PublicKeyType actual_key_type =
            net::X509Certificate::kPublicKeyTypeUnknown;
        size_t unused_key_size = 0;
        net::X509Certificate::GetPublicKeyInfo(
            certificate->cert_buffer(), &unused_key_size, &actual_key_type);
        const std::vector<net::X509Certificate::PublicKeyType>& accepted_types =
            request_.certificate_key_types;
        if (!base::Contains(accepted_types, actual_key_type))
          continue;
      }

      matches_pending_permissions_check_.push_back(std::move(certificate));
    }
    DoStep();
  }

  // This is called once for each certificate in
  // |matches_pending_permissions_check_|. Each invocation processes the first
  // element and removes it from the deque. Each processed certificate is added
  // to |matches_| if it is selectable according to
  // ExtensionKeyPermissionsService. When all certificates have been processed,
  // advances the SignTask state machine to |next_step|.
  void CheckKeyPermissions(Step next_step) {
    if (matches_pending_permissions_check_.empty()) {
      next_step_ = next_step;
      DoStep();
      return;
    }

    scoped_refptr<net::X509Certificate> certificate =
        std::move(matches_pending_permissions_check_.front());
    matches_pending_permissions_check_.pop_front();
    std::vector<uint8_t> public_key_spki_der =
        platform_keys::GetSubjectPublicKeyInfoBlob(certificate);

    extension_key_permissions_service_->CanUseKeyForSigning(
        public_key_spki_der,
        base::BindOnce(&SelectTask::OnKeySigningPermissionKnown,
                       weak_factory_.GetWeakPtr(), public_key_spki_der,
                       certificate));
  }

  void OnKeySigningPermissionKnown(
      std::vector<uint8_t> public_key_spki_der,
      const scoped_refptr<net::X509Certificate>& certificate,
      bool allowed) {
    if (allowed) {
      matches_.push_back(certificate);
      DoStep();
    } else if (interactive_) {
      service_->keystore_service_->CanUserGrantPermissionForKey(
          std::move(public_key_spki_der),
          base::BindOnce(&SelectTask::OnAbilityToGrantPermissionKnown,
                         weak_factory_.GetWeakPtr(), std::move(certificate)));
    } else {
      DoStep();
    }
  }

  void OnAbilityToGrantPermissionKnown(
      const scoped_refptr<net::X509Certificate>& certificate,
      bool allowed) {
    if (allowed) {
      matches_.push_back(certificate);
    }
    DoStep();
  }

  // If |input_client_certificates_| is given, removes from |matches_| all
  // certificates that are not elements of |input_client_certificates_|.
  void IntersectWithInputCerts() {
    if (!input_client_certificates_) {
      DoStep();
      return;
    }
    platform_keys::IntersectCertificates(
        matches_, *input_client_certificates_,
        base::BindOnce(&SelectTask::GotIntersection,
                       weak_factory_.GetWeakPtr()));
  }

  void GotIntersection(std::unique_ptr<net::CertificateList> intersection) {
    matches_.swap(*intersection);
    DoStep();
  }

  // Calls |service_->select_delegate_->Select()| to select a cert from
  // |matches_|, which will be stored in |selected_cert_|.
  // Will call back to |GotSelection()|.
  void SelectCerts() {
    CHECK(interactive_);
    if (matches_.empty()) {
      // Don't show a select dialog if no certificate is matching.
      DoStep();
      return;
    }
    service_->select_delegate_->Select(
        extension_id_, matches_,
        base::BindOnce(&SelectTask::GotSelection, weak_factory_.GetWeakPtr()),
        web_contents_, service_->browser_context_);
  }

  // Will be called by |SelectCerts()| with the selected cert or null if no cert
  // was selected.
  void GotSelection(scoped_refptr<net::X509Certificate> selected_cert) {
    selected_cert_ = selected_cert;
    DoStep();
  }

  // Updates the extension's state store about unlimited sign permission for the
  // selected cert. Does nothing if no cert was selected.
  void UpdatePermission() {
    CHECK(interactive_);
    if (!selected_cert_) {
      DoStep();
      return;
    }
    extension_key_permissions_service_->SetUserGrantedPermission(
        platform_keys::GetSubjectPublicKeyInfoBlob(selected_cert_),
        base::BindOnce(&SelectTask::OnPermissionsUpdated,
                       weak_factory_.GetWeakPtr()));
  }

  void OnPermissionsUpdated(bool is_error,
                            crosapi::mojom::KeystoreError error) {
    if (is_error) {
      LOG(WARNING) << "Error while updating permissions: "
                   << platform_keys::KeystoreErrorToString(error);
    }

    DoStep();
  }

  // Passes the filtered certs to |callback_|.
  void PassResultingCerts() {
    std::unique_ptr<net::CertificateList> selection(new net::CertificateList);
    if (interactive_) {
      if (selected_cert_)
        selection->push_back(selected_cert_);
    } else {
      selection->assign(matches_.begin(), matches_.end());
    }

    std::move(callback_).Run(std::move(selection), /*error=*/std::nullopt);
    DoStep();
  }

  Step next_step_ = Step::GET_EXTENSION_PERMISSIONS;

  std::deque<scoped_refptr<net::X509Certificate>>
      matches_pending_permissions_check_;
  net::CertificateList matches_;
  scoped_refptr<net::X509Certificate> selected_cert_;
  platform_keys::ClientCertificateRequest request_;
  std::unique_ptr<net::CertificateList> input_client_certificates_;
  const bool interactive_;
  const extensions::ExtensionId extension_id_;
  SelectCertificatesCallback callback_;
  const raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<platform_keys::ExtensionKeyPermissionsService>
      extension_key_permissions_service_;
  const raw_ptr<ExtensionPlatformKeysService> service_;
  base::WeakPtrFactory<SelectTask> weak_factory_{this};
};

ExtensionPlatformKeysService::SelectDelegate::SelectDelegate() = default;

ExtensionPlatformKeysService::SelectDelegate::~SelectDelegate() = default;

ExtensionPlatformKeysService::ExtensionPlatformKeysService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      keystore_service_(GetKeystoreService(browser_context_)) {
  DCHECK(browser_context);
}

ExtensionPlatformKeysService::~ExtensionPlatformKeysService() = default;

void ExtensionPlatformKeysService::SetSelectDelegate(
    std::unique_ptr<SelectDelegate> delegate) {
  select_delegate_ = std::move(delegate);
}

void ExtensionPlatformKeysService::GenerateRSAKey(
    platform_keys::TokenId token_id,
    unsigned int modulus_length,
    bool sw_backed,
    std::string extension_id,
    GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!keystore_service_) [[unlikely]] {
    std::move(callback).Run(/*public_key_spki_der=*/std::vector<uint8_t>(),
                            crosapi::mojom::KeystoreError::kMojoUnavailable);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (sw_backed) {
    // Software-backed RSA keys are only supported starting with KeyStore
    // interface version 16.
    // TODO(crbug.com/40793151): Remove this code with M-100.
    const int kSoftwareBackedRsaMinVersion = 16;
    if (!chromeos::LacrosService::Get() ||
        (chromeos::LacrosService::Get()
             ->GetInterfaceVersion<KeystoreService>() <
         kSoftwareBackedRsaMinVersion)) {
      std::move(callback).Run(
          /*public_key_spki_der=*/std::vector<uint8_t>(),
          crosapi::mojom::KeystoreError::kUnsupportedKeyType);
      return;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  StartOrQueueTask(std::make_unique<GenerateRSAKeyTask>(
      token_id, modulus_length, sw_backed, std::move(extension_id),
      std::move(callback), this));
}

void ExtensionPlatformKeysService::GenerateECKey(
    platform_keys::TokenId token_id,
    std::string named_curve,
    std::string extension_id,
    GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!keystore_service_) [[unlikely]] {
    std::move(callback).Run(/*public_key_spki_der=*/std::vector<uint8_t>(),
                            crosapi::mojom::KeystoreError::kMojoUnavailable);
    return;
  }

  StartOrQueueTask(std::make_unique<GenerateECKeyTask>(
      token_id, std::move(named_curve), std::move(extension_id),
      std::move(callback), this));
}

bool ExtensionPlatformKeysService::IsUsingSigninProfile() {
// TODO(crbug.com/40156265) Revisit this place when Lacros-Chrome starts being
// used on the login screen.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::ProfileHelper::IsSigninProfile(
      Profile::FromBrowserContext(browser_context_));
#else
  return false;
#endif
}

void ExtensionPlatformKeysService::SignDigest(
    std::optional<platform_keys::TokenId> token_id,
    std::vector<uint8_t> data,
    std::vector<uint8_t> public_key_spki_der,
    platform_keys::KeyType key_type,
    platform_keys::HashAlgorithm hash_algorithm,
    std::string extension_id,
    SignCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!keystore_service_) [[unlikely]] {
    std::move(callback).Run(/*signature=*/std::vector<uint8_t>(),
                            crosapi::mojom::KeystoreError::kMojoUnavailable);
    return;
  }

  StartOrQueueTask(std::make_unique<SignTask>(
      token_id, std::move(data), std::move(public_key_spki_der), key_type,
      hash_algorithm, std::move(extension_id), std::move(callback), this));
}

void ExtensionPlatformKeysService::SignRSAPKCS1Raw(
    std::optional<platform_keys::TokenId> token_id,
    std::vector<uint8_t> data,
    std::vector<uint8_t> public_key_spki_der,
    std::string extension_id,
    SignCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!keystore_service_) [[unlikely]] {
    std::move(callback).Run(/*signature=*/std::vector<uint8_t>(),
                            crosapi::mojom::KeystoreError::kMojoUnavailable);
    return;
  }

  StartOrQueueTask(std::make_unique<SignTask>(
      token_id, std::move(data), std::move(public_key_spki_der),
      /*key_type=*/platform_keys::KeyType::kRsassaPkcs1V15,
      platform_keys::HASH_ALGORITHM_NONE, std::move(extension_id),
      std::move(callback), this));
}

void ExtensionPlatformKeysService::SelectClientCertificates(
    const platform_keys::ClientCertificateRequest& request,
    std::unique_ptr<net::CertificateList> client_certificates,
    bool interactive,
    std::string extension_id,
    SelectCertificatesCallback callback,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!keystore_service_) [[unlikely]] {
    std::move(callback).Run(/*matches=*/nullptr,
                            crosapi::mojom::KeystoreError::kMojoUnavailable);
    return;
  }

  StartOrQueueTask(std::make_unique<SelectTask>(
      request, std::move(client_certificates), interactive,
      std::move(extension_id), std::move(callback), web_contents, this));
}

void ExtensionPlatformKeysService::StartOrQueueTask(
    std::unique_ptr<Task> task) {
  tasks_.push(std::move(task));
  if (tasks_.size() == 1)
    tasks_.front()->Start();
}

void ExtensionPlatformKeysService::TaskFinished(Task* task) {
  DCHECK(!tasks_.empty());
  DCHECK(task == tasks_.front().get());
  // Remove all finished tasks from the queue (should be at most one).
  while (!tasks_.empty() && tasks_.front()->IsDone())
    tasks_.pop();

  // Now either the queue is empty or the next task is not finished yet and it
  // can be started.
  if (!tasks_.empty())
    tasks_.front()->Start();
}

}  // namespace chromeos
