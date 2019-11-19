// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/state_store.h"
#include "net/cert/x509_certificate.h"

using content::BrowserThread;

namespace chromeos {

namespace {

const char kErrorKeyNotAllowedForSigning[] =
    "This key is not allowed for signing. Either it was used for signing "
    "before or it was not correctly generated.";

// Converts |token_ids| (string-based token identifiers used in the
// platformKeys API) to a vector of KeyPermissions::KeyLocation. Currently only
// accepts |kTokenIdUser| and |kTokenIdSystem| as |token_ids| elements.
std::vector<KeyPermissions::KeyLocation> TokenIdsToKeyLocations(
    const std::vector<std::string>& token_ids) {
  std::vector<KeyPermissions::KeyLocation> key_locations;
  for (const auto& token_id : token_ids) {
    if (token_id == platform_keys::kTokenIdUser)
      key_locations.push_back(KeyPermissions::KeyLocation::kUserSlot);
    else if (token_id == platform_keys::kTokenIdSystem)
      key_locations.push_back(KeyPermissions::KeyLocation::kSystemSlot);
    else
      NOTREACHED() << "Unknown platformKeys API token id " << token_id;
  }
  return key_locations;
}

}  // namespace

class PlatformKeysService::Task {
 public:
  Task() {}
  virtual ~Task() {}
  virtual void Start() = 0;
  virtual bool IsDone() = 0;

 private:
  DISALLOW_ASSIGN(Task);
};

class PlatformKeysService::GenerateRSAKeyTask : public Task {
 public:
  enum class Step {
    GENERATE_KEY,
    GET_EXTENSION_PERMISSIONS,
    UPDATE_PERMISSIONS_AND_CALLBACK,
    DONE,
  };

  // This Task generates an RSA key with the parameters |token_id| and
  // |modulus_length| and registers it for the extension with id |extension_id|.
  // The generated key will be passed to |callback|.
  GenerateRSAKeyTask(const std::string& token_id,
                     unsigned int modulus_length,
                     const std::string& extension_id,
                     const GenerateKeyCallback& callback,
                     KeyPermissions* key_permissions,
                     PlatformKeysService* service,
                     content::BrowserContext* browser_context)
      : token_id_(token_id),
        modulus_length_(modulus_length),
        extension_id_(extension_id),
        callback_(callback),
        key_permissions_(key_permissions),
        service_(service),
        browser_context_(browser_context) {}

  ~GenerateRSAKeyTask() override {}

  void Start() override {
    CHECK(next_step_ == Step::GENERATE_KEY);
    DoStep();
  }

  bool IsDone() override { return next_step_ == Step::DONE; }

 private:
  void DoStep() {
    switch (next_step_) {
      case Step::GENERATE_KEY:
        next_step_ = Step::GET_EXTENSION_PERMISSIONS;
        GenerateKey();
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

  // Generates the RSA key.
  void GenerateKey() {
    platform_keys::subtle::GenerateRSAKey(
        token_id_, modulus_length_,
        base::Bind(&GenerateRSAKeyTask::GeneratedKey,
                   weak_factory_.GetWeakPtr()),
        browser_context_);
  }

  // Stores the generated key or in case of an error calls |callback_| with the
  // error message.
  void GeneratedKey(const std::string& public_key_spki_der,
                    const std::string& error_message) {
    if (!error_message.empty()) {
      next_step_ = Step::DONE;
      callback_.Run(std::string() /* no public key */, error_message);
      DoStep();
      return;
    }
    public_key_spki_der_ = public_key_spki_der;
    DoStep();
  }

  // Gets the permissions for the extension with id |extension_id|.
  void GetExtensionPermissions() {
    key_permissions_->GetPermissionsForExtension(
        extension_id_, base::Bind(&GenerateRSAKeyTask::GotPermissions,
                                  base::Unretained(this)));
  }

  void UpdatePermissionsAndCallBack() {
    std::vector<KeyPermissions::KeyLocation> key_locations =
        TokenIdsToKeyLocations({token_id_});
    extension_permissions_->RegisterKeyForCorporateUsage(public_key_spki_der_,
                                                         key_locations);
    callback_.Run(public_key_spki_der_, std::string() /* no error */);
    DoStep();
    return;
  }

  void GotPermissions(std::unique_ptr<KeyPermissions::PermissionsForExtension>
                          extension_permissions) {
    extension_permissions_ = std::move(extension_permissions);
    DoStep();
  }

  Step next_step_ = Step::GENERATE_KEY;

  const std::string token_id_;
  const unsigned int modulus_length_;
  std::string public_key_spki_der_;
  const std::string extension_id_;
  GenerateKeyCallback callback_;
  std::unique_ptr<KeyPermissions::PermissionsForExtension>
      extension_permissions_;
  KeyPermissions* const key_permissions_;
  PlatformKeysService* const service_;
  content::BrowserContext* const browser_context_;
  base::WeakPtrFactory<GenerateRSAKeyTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GenerateRSAKeyTask);
};

class PlatformKeysService::SignTask : public Task {
 public:
  enum class Step {
    GET_EXTENSION_PERMISSIONS,
    GET_KEY_LOCATIONS,
    SIGN_OR_ABORT,
    DONE,
  };

  // This Task will check the permissions of the extension with |extension_id|
  // for the key identified by |public_key_spki_der|. If the permission check
  // was positive, signs |data| with the key and passes the signature to
  // |callback|. If the extension is not allowed to use the key multiple times,
  // also updates the permission to prevent any future signing operation of that
  // extension using that same key.
  // If an error occurs, an error message is passed to |callback| instead.
  SignTask(const std::string& token_id,
           const std::string& data,
           const std::string& public_key_spki_der,
           bool sign_direct_pkcs_padded,
           platform_keys::HashAlgorithm hash_algorithm,
           const std::string& extension_id,
           const SignCallback& callback,
           KeyPermissions* key_permissions,
           PlatformKeysService* service)
      : token_id_(token_id),
        data_(data),
        public_key_spki_der_(public_key_spki_der),
        sign_direct_pkcs_padded_(sign_direct_pkcs_padded),
        hash_algorithm_(hash_algorithm),
        extension_id_(extension_id),
        callback_(callback),
        key_permissions_(key_permissions),
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
        next_step_ = Step::GET_KEY_LOCATIONS;
        GetExtensionPermissions();
        return;
      case Step::GET_KEY_LOCATIONS:
        next_step_ = Step::SIGN_OR_ABORT;
        GetKeyLocations();
        return;
      case Step::SIGN_OR_ABORT: {
        next_step_ = Step::DONE;
        bool sign_granted = extension_permissions_->CanUseKeyForSigning(
            public_key_spki_der_, key_locations_);
        if (sign_granted) {
          Sign();
        } else {
          callback_.Run(std::string() /* no signature */,
                        kErrorKeyNotAllowedForSigning);
          DoStep();
        }
        return;
      }
      case Step::DONE:
        service_->TaskFinished(this);
        // |this| might be invalid now.
        return;
    }
  }

  void GetExtensionPermissions() {
    key_permissions_->GetPermissionsForExtension(
        extension_id_,
        base::Bind(&SignTask::GotPermissions, base::Unretained(this)));
  }

  void GotPermissions(std::unique_ptr<KeyPermissions::PermissionsForExtension>
                          extension_permissions) {
    extension_permissions_ = std::move(extension_permissions);
    DoStep();
  }

  void GetKeyLocations() {
    platform_keys::GetKeyLocations(
        public_key_spki_der_,
        base::BindRepeating(&SignTask::GotKeyLocation, base::Unretained(this)),
        service_->browser_context_);
  }

  void GotKeyLocation(const std::vector<std::string>& token_ids,
                      const std::string& error_message) {
    if (!error_message.empty()) {
      next_step_ = Step::DONE;
      callback_.Run(std::string() /* no signature */, error_message);
      DoStep();
      return;
    }

    key_locations_ = TokenIdsToKeyLocations(token_ids);
    DoStep();
  }

  // Updates the permissions for |public_key_spki_der_|, starts the actual
  // signing operation and afterwards passes the signature (or error) to
  // |callback_|.
  void Sign() {
    extension_permissions_->SetKeyUsedForSigning(public_key_spki_der_,
                                                 key_locations_);

    if (sign_direct_pkcs_padded_) {
      platform_keys::subtle::SignRSAPKCS1Raw(
          token_id_, data_, public_key_spki_der_,
          base::Bind(&SignTask::DidSign, weak_factory_.GetWeakPtr()),
          service_->browser_context_);
    } else {
      platform_keys::subtle::SignRSAPKCS1Digest(
          token_id_, data_, public_key_spki_der_, hash_algorithm_,
          base::Bind(&SignTask::DidSign, weak_factory_.GetWeakPtr()),
          service_->browser_context_);
    }
  }

  void DidSign(const std::string& signature, const std::string& error_message) {
    callback_.Run(signature, error_message);
    DoStep();
  }

  Step next_step_ = Step::GET_EXTENSION_PERMISSIONS;

  const std::string token_id_;
  const std::string data_;
  const std::string public_key_spki_der_;

  // If true, |data_| will not be hashed before signing. Only PKCS#1 v1.5
  // padding will be applied before signing.
  // If false, |hash_algorithm_| is set to a value != NONE.
  const bool sign_direct_pkcs_padded_;
  const platform_keys::HashAlgorithm hash_algorithm_;
  const std::string extension_id_;
  const SignCallback callback_;
  std::unique_ptr<KeyPermissions::PermissionsForExtension>
      extension_permissions_;
  KeyPermissions* const key_permissions_;
  std::vector<KeyPermissions::KeyLocation> key_locations_;
  PlatformKeysService* const service_;
  base::WeakPtrFactory<SignTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SignTask);
};

class PlatformKeysService::SelectTask : public Task {
 public:
  enum class Step {
    GET_EXTENSION_PERMISSIONS,
    GET_MATCHING_CERTS,
    GET_KEY_LOCATIONS,
    INTERSECT_WITH_INPUT_CERTS,
    SELECT_CERTS,
    UPDATE_PERMISSION,
    FILTER_BY_PERMISSIONS,
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
             const SelectCertificatesCallback& callback,
             content::WebContents* web_contents,
             KeyPermissions* key_permissions,
             PlatformKeysService* service)
      : request_(request),
        input_client_certificates_(std::move(input_client_certificates)),
        interactive_(interactive),
        extension_id_(extension_id),
        callback_(callback),
        web_contents_(web_contents),
        key_permissions_(key_permissions),
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
        next_step_ = Step::GET_KEY_LOCATIONS;
        GetMatchingCerts();
        return;
      case Step::GET_KEY_LOCATIONS:
        // Don't advance to the next step yet - GetKeyLocations is repeated for
        // all matching certs. The next step will be selected in
        // GetKeyLocations.
        GetKeyLocations(Step::INTERSECT_WITH_INPUT_CERTS /* next_step */);
        return;
      case Step::INTERSECT_WITH_INPUT_CERTS:
        if (interactive_)
          next_step_ = Step::SELECT_CERTS;
        else  // Skip SelectCerts and UpdatePermission if not interactive.
          next_step_ = Step::FILTER_BY_PERMISSIONS;
        IntersectWithInputCerts();
        return;
      case Step::SELECT_CERTS:
        next_step_ = Step::UPDATE_PERMISSION;
        SelectCerts();
        return;
      case Step::UPDATE_PERMISSION:
        next_step_ = Step::FILTER_BY_PERMISSIONS;
        UpdatePermission();
        return;
      case Step::FILTER_BY_PERMISSIONS:
        next_step_ = Step::DONE;
        FilterSelectionByPermission();
        return;
      case Step::DONE:
        service_->TaskFinished(this);
        // |this| might be invalid now.
        return;
    }
  }

  void GetExtensionPermissions() {
    key_permissions_->GetPermissionsForExtension(
        extension_id_,
        base::Bind(&SelectTask::GotPermissions, base::Unretained(this)));
  }

  void GotPermissions(std::unique_ptr<KeyPermissions::PermissionsForExtension>
                          extension_permissions) {
    extension_permissions_ = std::move(extension_permissions);
    DoStep();
  }

  // Retrieves all certificates matching |request_|. Will call back to
  // |GotMatchingCerts()|.
  void GetMatchingCerts() {
    platform_keys::subtle::SelectClientCertificates(
        request_.certificate_authorities,
        base::Bind(&SelectTask::GotMatchingCerts, weak_factory_.GetWeakPtr()),
        service_->browser_context_);
  }

  // If the certificate request could be processed successfully, |matches| will
  // contain the list of matching certificates (maybe empty) and |error_message|
  // will be empty. If an error occurred, |matches| will be null and
  // |error_message| contain an error message.
  // Note that the order of |matches|, based on the expiration/issuance date, is
  // relevant and must be preserved in any processing of the list.
  void GotMatchingCerts(std::unique_ptr<net::CertificateList> matches,
                        const std::string& error_message) {
    if (!error_message.empty()) {
      next_step_ = Step::DONE;
      callback_.Run(nullptr /* no certificates */, error_message);
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

      matches_pending_key_locations_.push_back(std::move(certificate));
    }
    DoStep();
  }

  // This is called once for each certificate in
  // |matches_pending_key_locations_|.  Each invocation processes the first
  // element and removes it from the deque. Each processed certificate is added
  // to |matches_| and |key_locations_for_matches_| if it is selectable
  // according to KeyPermissions. When all certificates have been processed,
  // advances the SignTask state machine to |next_step|.
  void GetKeyLocations(Step next_step) {
    if (matches_pending_key_locations_.empty()) {
      next_step_ = next_step;
      DoStep();
      return;
    }

    scoped_refptr<net::X509Certificate> certificate =
        std::move(matches_pending_key_locations_.front());
    matches_pending_key_locations_.pop_front();
    const std::string public_key_spki_der(
        platform_keys::GetSubjectPublicKeyInfo(certificate));

    platform_keys::GetKeyLocations(
        public_key_spki_der,
        base::BindRepeating(&SelectTask::GotKeyLocations,
                            base::Unretained(this), certificate),
        service_->browser_context_);
  }

  void GotKeyLocations(const scoped_refptr<net::X509Certificate>& certificate,
                       const std::vector<std::string>& token_ids,
                       const std::string& error_message) {
    if (!error_message.empty()) {
      next_step_ = Step::DONE;
      callback_.Run(nullptr /* no certificates */, error_message);
      DoStep();
      return;
    }

    const std::string public_key_spki_der(
        platform_keys::GetSubjectPublicKeyInfo(certificate));

    std::vector<KeyPermissions::KeyLocation> key_locations =
        TokenIdsToKeyLocations(token_ids);

    // Use this key if the user can use it for signing or can grant permission
    // for it.
    if (key_permissions_->CanUserGrantPermissionFor(public_key_spki_der,
                                                    key_locations) ||
        extension_permissions_->CanUseKeyForSigning(public_key_spki_der,
                                                    key_locations)) {
      matches_.push_back(certificate);
      key_locations_for_matches_[public_key_spki_der] = key_locations;
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
        base::Bind(&SelectTask::GotIntersection, weak_factory_.GetWeakPtr()));
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
        base::Bind(&SelectTask::GotSelection, base::Unretained(this)),
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
    auto key_locations_iter =
        key_locations_for_matches_.find(public_key_spki_der);
    CHECK(key_locations_iter != key_locations_for_matches_.end());
    extension_permissions_->SetUserGrantedPermission(
        public_key_spki_der, key_locations_iter->second);
    DoStep();
  }

  // Filters from all matches (if not interactive) or from the selection (if
  // interactive), the certificates that the extension has unlimited sign
  // permission for. Passes the filtered certs to |callback_|.
  void FilterSelectionByPermission() {
    std::unique_ptr<net::CertificateList> selection(new net::CertificateList);
    if (interactive_) {
      if (selected_cert_)
        selection->push_back(selected_cert_);
    } else {
      selection->assign(matches_.begin(), matches_.end());
    }

    std::unique_ptr<net::CertificateList> filtered_certs(
        new net::CertificateList);
    for (scoped_refptr<net::X509Certificate> selected_cert : *selection) {
      const std::string public_key_spki_der(
          platform_keys::GetSubjectPublicKeyInfo(selected_cert));

      auto key_locations_iter =
          key_locations_for_matches_.find(public_key_spki_der);
      CHECK(key_locations_iter != key_locations_for_matches_.end());
      if (!extension_permissions_->CanUseKeyForSigning(
              public_key_spki_der, key_locations_iter->second))
        continue;

      filtered_certs->push_back(selected_cert);
    }
    // Note: In the interactive case this should have filtered exactly the
    // one selected cert. Checking the permissions again is not striclty
    // necessary but this ensures that the permissions were updated correctly.
    CHECK(!selected_cert_ || (filtered_certs->size() == 1 &&
                              filtered_certs->front() == selected_cert_));
    callback_.Run(std::move(filtered_certs), std::string() /* no error */);
    DoStep();
  }

  Step next_step_ = Step::GET_EXTENSION_PERMISSIONS;

  std::deque<scoped_refptr<net::X509Certificate>>
      matches_pending_key_locations_;
  net::CertificateList matches_;
  // Mapping of DER-encoded Subject Public Key Info to the KeyLocations
  // determined for the corresponding private key.
  base::flat_map<std::string, std::vector<KeyPermissions::KeyLocation>>
      key_locations_for_matches_;
  scoped_refptr<net::X509Certificate> selected_cert_;
  platform_keys::ClientCertificateRequest request_;
  std::unique_ptr<net::CertificateList> input_client_certificates_;
  const bool interactive_;
  const std::string extension_id_;
  const SelectCertificatesCallback callback_;
  content::WebContents* const web_contents_;
  std::unique_ptr<KeyPermissions::PermissionsForExtension>
      extension_permissions_;
  KeyPermissions* const key_permissions_;
  PlatformKeysService* const service_;
  base::WeakPtrFactory<SelectTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SelectTask);
};

PlatformKeysService::SelectDelegate::SelectDelegate() {
}

PlatformKeysService::SelectDelegate::~SelectDelegate() {
}

PlatformKeysService::PlatformKeysService(
    bool profile_is_managed,
    PrefService* profile_prefs,
    policy::PolicyService* profile_policies,
    content::BrowserContext* browser_context,
    extensions::StateStore* state_store)
    : browser_context_(browser_context),
      key_permissions_(profile_is_managed,
                       profile_prefs,
                       profile_policies,
                       state_store) {
  DCHECK(browser_context);
  DCHECK(state_store);
}

PlatformKeysService::~PlatformKeysService() {
}

void PlatformKeysService::SetSelectDelegate(
    std::unique_ptr<SelectDelegate> delegate) {
  select_delegate_ = std::move(delegate);
}

void PlatformKeysService::GenerateRSAKey(const std::string& token_id,
                                         unsigned int modulus_length,
                                         const std::string& extension_id,
                                         const GenerateKeyCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartOrQueueTask(std::make_unique<GenerateRSAKeyTask>(
      token_id, modulus_length, extension_id, callback, &key_permissions_, this,
      browser_context_));
}

void PlatformKeysService::SignRSAPKCS1Digest(
    const std::string& token_id,
    const std::string& data,
    const std::string& public_key_spki_der,
    platform_keys::HashAlgorithm hash_algorithm,
    const std::string& extension_id,
    const SignCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartOrQueueTask(base::WrapUnique(new SignTask(
      token_id, data, public_key_spki_der, false /* digest before signing */,
      hash_algorithm, extension_id, callback, &key_permissions_, this)));
}

void PlatformKeysService::SignRSAPKCS1Raw(
    const std::string& token_id,
    const std::string& data,
    const std::string& public_key_spki_der,
    const std::string& extension_id,
    const SignCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartOrQueueTask(base::WrapUnique(
      new SignTask(token_id, data, public_key_spki_der,
                   true /* sign directly without hashing */,
                   platform_keys::HASH_ALGORITHM_NONE, extension_id, callback,
                   &key_permissions_, this)));
}

void PlatformKeysService::SelectClientCertificates(
    const platform_keys::ClientCertificateRequest& request,
    std::unique_ptr<net::CertificateList> client_certificates,
    bool interactive,
    const std::string& extension_id,
    const SelectCertificatesCallback& callback,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartOrQueueTask(std::make_unique<SelectTask>(
      request, std::move(client_certificates), interactive, extension_id,
      callback, web_contents, &key_permissions_, this));
}

void PlatformKeysService::StartOrQueueTask(std::unique_ptr<Task> task) {
  tasks_.push(std::move(task));
  if (tasks_.size() == 1)
    tasks_.front()->Start();
}

void PlatformKeysService::TaskFinished(Task* task) {
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
