// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/extension_key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/extension_key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "net/cert/x509_certificate.h"

using content::BrowserThread;

namespace chromeos {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Verify the allowlisted kKeyPermissionsInLoginScreen feature behaviors.
bool IsExtensionAllowlisted(const extensions::Extension* extension) {
  // Can be nullptr if the extension is uninstalled before the SignTask is
  // completed.
  if (!extension)
    return false;

  const extensions::Feature* key_permissions_in_login_screen =
      extensions::FeatureProvider::GetBehaviorFeature(
          extensions::behavior_feature::kKeyPermissionsInLoginScreen);

  return key_permissions_in_login_screen->IsAvailableToExtension(extension)
      .is_available();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

class ExtensionPlatformKeysService::Task {
 public:
  Task() {}
  virtual ~Task() {}
  virtual void Start() = 0;
  virtual bool IsDone() = 0;

 private:
  DISALLOW_ASSIGN(Task);
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
                  const std::string& extension_id,
                  GenerateKeyCallback callback,
                  ExtensionPlatformKeysService* service)
      : token_id_(token_id),
        extension_id_(extension_id),
        callback_(std::move(callback)),
        service_(service) {}

  ~GenerateKeyTask() override = default;

  void Start() override {
    CHECK(next_step_ == Step::GENERATE_KEY);
    DoStep();
  }

  bool IsDone() override { return next_step_ == Step::DONE; }

 protected:
  virtual void GenerateKey(GenerateKeyCallback callback) = 0;

  platform_keys::TokenId token_id_;
  std::string public_key_spki_der_;
  const std::string extension_id_;
  GenerateKeyCallback callback_;
  std::unique_ptr<platform_keys::ExtensionKeyPermissionsService>
      extension_key_permissions_service_;
  ExtensionPlatformKeysService* const service_;

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
  void GeneratedKey(const std::string& public_key_spki_der,
                    platform_keys::Status status) {
    if (status != platform_keys::Status::kSuccess) {
      next_step_ = Step::DONE;
      std::move(callback_).Run(std::string() /* no public key */, status);
      DoStep();
      return;
    }
    public_key_spki_der_ = public_key_spki_der;
    DoStep();
  }

  // Gets the permissions for the extension with id |extension_id|.
  void GetExtensionPermissions() {
    platform_keys::ExtensionKeyPermissionsServiceFactory::
        GetForBrowserContextAndExtension(
            base::BindOnce(&GenerateKeyTask::GotPermissions,
                           base::Unretained(this)),
            service_->browser_context_, extension_id_,
            service_->key_permissions_service_);
  }

  void OnKeyRegisteredForCorporateUsage(platform_keys::Status status) {
    if (status == platform_keys::Status::kSuccess) {
      std::move(callback_).Run(public_key_spki_der_, status);
      DoStep();
      return;
    }

    LOG(ERROR) << "Corporate key registration failed: "
               << platform_keys::StatusToString(status);

    service_->platform_keys_service_->RemoveKey(
        token_id_, public_key_spki_der_,
        base::BindOnce(&GenerateKeyTask::RemoveKeyCallback,
                       base::Unretained(this),
                       /*corporate_key_registration_error_status=*/status));
  }

  void RemoveKeyCallback(
      platform_keys::Status corporate_key_registration_error_status,
      platform_keys::Status remove_key_status) {
    if (remove_key_status != platform_keys::Status::kSuccess) {
      LOG(ERROR)
          << "Failed to remove a dangling key with error: "
          << platform_keys::StatusToString(remove_key_status)
          << ", after failing to register key for corporate usage with error: "
          << platform_keys::StatusToString(
                 corporate_key_registration_error_status);
    }

    next_step_ = Step::DONE;
    std::move(callback_).Run(std::string() /* no public key */,
                             corporate_key_registration_error_status);
    DoStep();
  }

  void UpdatePermissionsAndCallBack() {
    extension_key_permissions_service_->RegisterKeyForCorporateUsage(
        public_key_spki_der_,
        base::BindOnce(&GenerateKeyTask::OnKeyRegisteredForCorporateUsage,
                       base::Unretained(this)));
  }

  void GotPermissions(
      std::unique_ptr<platform_keys::ExtensionKeyPermissionsService>
          extension_key_permissions_service) {
    extension_key_permissions_service_ =
        std::move(extension_key_permissions_service);
    DoStep();
  }

  Step next_step_ = Step::GENERATE_KEY;

  base::WeakPtrFactory<GenerateKeyTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GenerateKeyTask);
};

class ExtensionPlatformKeysService::GenerateRSAKeyTask
    : public GenerateKeyTask {
 public:
  // This key task generates an RSA key with the parameters |token_id| and
  // |modulus_length| and registers it for the extension with id |extension_id|.
  // The generated key will be passed to |callback|.
  GenerateRSAKeyTask(platform_keys::TokenId token_id,
                     unsigned int modulus_length,
                     const std::string& extension_id,
                     GenerateKeyCallback callback,
                     ExtensionPlatformKeysService* service)
      : GenerateKeyTask(token_id, extension_id, std::move(callback), service),
        modulus_length_(modulus_length) {}

  ~GenerateRSAKeyTask() override {}

 private:
  // Generates the RSA key.
  void GenerateKey(GenerateKeyCallback callback) override {
    service_->platform_keys_service_->GenerateRSAKey(token_id_, modulus_length_,
                                                     std::move(callback));
  }

  const unsigned int modulus_length_;
};

class ExtensionPlatformKeysService::GenerateECKeyTask : public GenerateKeyTask {
 public:
  // This Task generates an EC key with the parameters |token_id| and
  // |named_curve| and registers it for the extension with id |extension_id|.
  // The generated key will be passed to |callback|.
  GenerateECKeyTask(platform_keys::TokenId token_id,
                    const std::string& named_curve,
                    const std::string& extension_id,
                    GenerateKeyCallback callback,
                    ExtensionPlatformKeysService* service)
      : GenerateKeyTask(token_id, extension_id, std::move(callback), service),
        named_curve_(named_curve) {}

  ~GenerateECKeyTask() override {}

 private:
  // Generates the EC key.
  void GenerateKey(GenerateKeyCallback callback) override {
    service_->platform_keys_service_->GenerateECKey(token_id_, named_curve_,
                                                    std::move(callback));
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
  SignTask(base::Optional<platform_keys::TokenId> token_id,
           const std::string& data,
           const std::string& public_key_spki_der,
           bool raw_pkcs1,
           platform_keys::KeyType key_type,
           platform_keys::HashAlgorithm hash_algorithm,
           const std::string& extension_id,
           SignCallback callback,
           ExtensionPlatformKeysService* service)
      : token_id_(token_id),
        data_(data),
        public_key_spki_der_(public_key_spki_der),
        raw_pkcs1_(raw_pkcs1),
        key_type_(key_type),
        hash_algorithm_(hash_algorithm),
        extension_id_(extension_id),
        callback_(std::move(callback)),
        service_(service) {}

  ~SignTask() override {}

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
            base::BindOnce(&SignTask::GotPermissions, base::Unretained(this)),
            service_->browser_context_, extension_id_,
            service_->key_permissions_service_);
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
            ->GetExtensionById(extension_id_,
                               extensions::ExtensionRegistry::ENABLED);
    if (service_->IsUsingSigninProfile() && IsExtensionAllowlisted(extension)) {
      DoStep();
      return;
    }

    extension_key_permissions_service_->CanUseKeyForSigning(
        public_key_spki_der_,
        base::BindOnce(&SignTask::OnCanUseKeyForSigningKnown,
                       base::Unretained(this)));
  }

  void OnCanUseKeyForSigningKnown(bool allowed) {
    if (!allowed) {
      std::move(callback_).Run(
          std::string() /* no signature */,
          platform_keys::Status::kErrorKeyNotAllowedForSigning);
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
                       base::Unretained(this)));
  }

  void OnSetKeyUsedForSigningDone(platform_keys::Status status) {
    if (status != platform_keys::Status::kSuccess) {
      LOG(ERROR) << "Marking a key used for signing failed: "
                 << platform_keys::StatusToString(status);
      next_step_ = Step::DONE;
      std::move(callback_).Run(std::string() /* no signature */, status);
      DoStep();
      return;
    }

    DoStep();
  }

  // Starts the actual signing operation and afterwards passes the signature (or
  // error) to |callback_|.
  void Sign() {
    switch (key_type_) {
      case platform_keys::KeyType::kRsassaPkcs1V15: {
        if (raw_pkcs1_) {
          service_->platform_keys_service_->SignRSAPKCS1Raw(
              token_id_, data_, public_key_spki_der_,
              base::BindOnce(&SignTask::DidSign, weak_factory_.GetWeakPtr()));
        } else {
          service_->platform_keys_service_->SignRSAPKCS1Digest(
              token_id_, data_, public_key_spki_der_, hash_algorithm_,

              base::BindOnce(&SignTask::DidSign, weak_factory_.GetWeakPtr()));
        }
        break;
      }
      case platform_keys::KeyType::kEcdsa: {
        service_->platform_keys_service_->SignECDSADigest(
            token_id_, data_, public_key_spki_der_, hash_algorithm_,
            base::BindOnce(&SignTask::DidSign, weak_factory_.GetWeakPtr()));
        break;
      }
    }
  }

  void DidSign(const std::string& signature, platform_keys::Status status) {
    std::move(callback_).Run(signature, status);
    DoStep();
  }

  Step next_step_ = Step::GET_EXTENSION_PERMISSIONS;

  base::Optional<platform_keys::TokenId> token_id_;
  const std::string data_;
  const std::string public_key_spki_der_;

  // If true, |data_| will not be hashed before signing. Only PKCS#1 v1.5
  // padding will be applied before signing.
  // If false, |hash_algorithm_| is set to a value != NONE.
  bool raw_pkcs1_;
  const platform_keys::KeyType key_type_;
  const platform_keys::HashAlgorithm hash_algorithm_;
  const std::string extension_id_;
  SignCallback callback_;
  std::unique_ptr<platform_keys::ExtensionKeyPermissionsService>
      extension_key_permissions_service_;
  ExtensionPlatformKeysService* const service_;
  base::WeakPtrFactory<SignTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SignTask);
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
             const std::string& extension_id,
             SelectCertificatesCallback callback,
             content::WebContents* web_contents,
             ExtensionPlatformKeysService* service)
      : request_(request),
        input_client_certificates_(std::move(input_client_certificates)),
        interactive_(interactive),
        extension_id_(extension_id),
        callback_(std::move(callback)),
        web_contents_(web_contents),
        service_(service) {}
  ~SelectTask() override {}

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
            base::BindOnce(&SelectTask::GotPermissions, base::Unretained(this)),
            service_->browser_context_, extension_id_,
            service_->key_permissions_service_);
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
    service_->platform_keys_service_->SelectClientCertificates(
        request_.certificate_authorities,
        base::BindOnce(&SelectTask::GotMatchingCerts,
                       weak_factory_.GetWeakPtr()));
  }

  // If the certificate request could be processed successfully, |matches| will
  // contain the list of matching certificates (maybe empty). If an error
  // occurred, |matches| will be null. Note that the order of |matches|, based
  // on the expiration/issuance date, is relevant and must be preserved in any
  // processing of the list.
  void GotMatchingCerts(std::unique_ptr<net::CertificateList> matches,
                        platform_keys::Status status) {
    if (status != platform_keys::Status::kSuccess) {
      next_step_ = Step::DONE;
      std::move(callback_).Run(nullptr /* no certificates */, status);
      DoStep();
      return;
    }

    for (scoped_refptr<net::X509Certificate>& certificate : *matches) {
      // Filter the retrieved certificates returning only those whose type is
      // equal to one of the entries in the type field of the certificate
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
  // to |matches_| if it is selectable according to KeyPermissionsService. When
  // all certificates have been processed, advances the SignTask state machine
  // to |next_step|.
  void CheckKeyPermissions(Step next_step) {
    if (matches_pending_permissions_check_.empty()) {
      next_step_ = next_step;
      DoStep();
      return;
    }

    scoped_refptr<net::X509Certificate> certificate =
        std::move(matches_pending_permissions_check_.front());
    matches_pending_permissions_check_.pop_front();
    const std::string public_key_spki_der(
        platform_keys::GetSubjectPublicKeyInfo(certificate));

    extension_key_permissions_service_->CanUseKeyForSigning(
        public_key_spki_der,
        base::BindOnce(&SelectTask::OnKeySigningPermissionKnown,
                       base::Unretained(this), public_key_spki_der,
                       certificate));
  }

  void OnKeySigningPermissionKnown(
      const std::string& public_key_spki_der,
      const scoped_refptr<net::X509Certificate>& certificate,
      bool allowed) {
    if (allowed) {
      matches_.push_back(certificate);
      DoStep();
    } else if (interactive_) {
      platform_keys::KeyPermissionsService* const key_permissions_service =
          platform_keys::KeyPermissionsServiceFactory::GetForBrowserContext(
              service_->browser_context_);
      key_permissions_service->CanUserGrantPermissionForKey(
          public_key_spki_der,
          base::BindOnce(&SelectTask::OnAbilityToGrantPermissionKnown,
                         base::Unretained(this), std::move(certificate)));
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
        base::BindOnce(&SelectTask::GotSelection, base::Unretained(this)),
        web_contents_, service_->browser_context_);
  }

  // Will be called by |SelectCerts()| with the selected cert or null if no cert
  // was selected.
  void GotSelection(const scoped_refptr<net::X509Certificate>& selected_cert) {
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
    const std::string public_key_spki_der(
        platform_keys::GetSubjectPublicKeyInfo(selected_cert_));
    extension_key_permissions_service_->SetUserGrantedPermission(
        public_key_spki_der, base::BindOnce(&SelectTask::OnPermissionsUpdated,
                                            base::Unretained(this)));
  }

  void OnPermissionsUpdated(platform_keys::Status status) {
    if (status != platform_keys::Status::kSuccess) {
      LOG(WARNING) << "Error while updating permissions: "
                   << platform_keys::StatusToString(status);
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

    std::move(callback_).Run(std::move(selection),
                             platform_keys::Status::kSuccess);
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
  const std::string extension_id_;
  SelectCertificatesCallback callback_;
  content::WebContents* const web_contents_;
  std::unique_ptr<platform_keys::ExtensionKeyPermissionsService>
      extension_key_permissions_service_;
  ExtensionPlatformKeysService* const service_;
  base::WeakPtrFactory<SelectTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SelectTask);
};

ExtensionPlatformKeysService::SelectDelegate::SelectDelegate() {}

ExtensionPlatformKeysService::SelectDelegate::~SelectDelegate() {}

ExtensionPlatformKeysService::ExtensionPlatformKeysService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      platform_keys_service_(
          platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
              browser_context)),
      key_permissions_service_(
          chromeos::platform_keys::KeyPermissionsServiceFactory::
              GetForBrowserContext(browser_context)) {
  DCHECK(platform_keys_service_);
  DCHECK(browser_context);
}

ExtensionPlatformKeysService::~ExtensionPlatformKeysService() {}

void ExtensionPlatformKeysService::SetSelectDelegate(
    std::unique_ptr<SelectDelegate> delegate) {
  select_delegate_ = std::move(delegate);
}

void ExtensionPlatformKeysService::GenerateRSAKey(
    platform_keys::TokenId token_id,
    unsigned int modulus_length,
    const std::string& extension_id,
    GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartOrQueueTask(std::make_unique<GenerateRSAKeyTask>(
      token_id, modulus_length, extension_id, std::move(callback), this));
}

void ExtensionPlatformKeysService::GenerateECKey(
    platform_keys::TokenId token_id,
    const std::string& named_curve,
    const std::string& extension_id,
    GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartOrQueueTask(std::make_unique<GenerateECKeyTask>(
      token_id, named_curve, extension_id, std::move(callback), this));
}

bool ExtensionPlatformKeysService::IsUsingSigninProfile() {
  return ProfileHelper::IsSigninProfile(
      Profile::FromBrowserContext(browser_context_));
}

void ExtensionPlatformKeysService::SignDigest(
    base::Optional<platform_keys::TokenId> token_id,
    const std::string& data,
    const std::string& public_key_spki_der,
    platform_keys::KeyType key_type,
    platform_keys::HashAlgorithm hash_algorithm,
    const std::string& extension_id,
    SignCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartOrQueueTask(
      std::make_unique<SignTask>(token_id, data, public_key_spki_der,
                                 /*raw_pkcs1=*/false, key_type, hash_algorithm,
                                 extension_id, std::move(callback), this));
}

void ExtensionPlatformKeysService::SignRSAPKCS1Raw(
    base::Optional<platform_keys::TokenId> token_id,
    const std::string& data,
    const std::string& public_key_spki_der,
    const std::string& extension_id,
    SignCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartOrQueueTask(std::make_unique<SignTask>(
      token_id, data, public_key_spki_der,
      /*raw_pkcs1=*/true, /*key_type=*/platform_keys::KeyType::kRsassaPkcs1V15,
      platform_keys::HASH_ALGORITHM_NONE, extension_id, std::move(callback),
      this));
}

void ExtensionPlatformKeysService::SelectClientCertificates(
    const platform_keys::ClientCertificateRequest& request,
    std::unique_ptr<net::CertificateList> client_certificates,
    bool interactive,
    const std::string& extension_id,
    SelectCertificatesCallback callback,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartOrQueueTask(std::make_unique<SelectTask>(
      request, std::move(client_certificates), interactive, extension_id,
      std::move(callback), web_contents, this));
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
