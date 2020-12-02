// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/enterprise/cert_store/cert_store_service.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_impl.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "content/public/browser/browser_context.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_util_nss.h"

namespace arc {

namespace {

// Singleton factory for CertStoreService.
class CertStoreServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CertStoreService* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<CertStoreService*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static CertStoreServiceFactory* GetInstance() {
    return base::Singleton<CertStoreServiceFactory>::get();
  }

  CertStoreServiceFactory(const CertStoreServiceFactory&) = delete;
  CertStoreServiceFactory& operator=(const CertStoreServiceFactory&) = delete;

 private:
  friend base::DefaultSingletonTraits<CertStoreServiceFactory>;
  CertStoreServiceFactory()
      : BrowserContextKeyedServiceFactory(
            "CertStoreService",
            BrowserContextDependencyManager::GetInstance()) {}

  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return context;
  }

  bool ServiceIsNULLWhileTesting() const override { return true; }

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new CertStoreService(context);
  }
};

using IsCertificateAllowedCallback = base::OnceCallback<void(bool allowed)>;

void CheckKeyLocationAndCorporateFlag(
    IsCertificateAllowedCallback callback,
    const std::string& public_key_spki_der,
    content::BrowserContext* const context,
    base::Optional<bool> key_on_user_token,
    chromeos::platform_keys::Status is_key_on_token_status) {
  if (is_key_on_token_status != chromeos::platform_keys::Status::kSuccess) {
    LOG(WARNING) << "Error while checking key location: "
                 << chromeos::platform_keys::StatusToString(
                        is_key_on_token_status);
    std::move(callback).Run(/* allowed */ false);
    return;
  }

  DCHECK(key_on_user_token.has_value());

  if (!key_on_user_token.value_or(false)) {
    std::move(callback).Run(/* allowed */ false);
    return;
  }

  // Check if the key is marked for corporate usage.
  chromeos::platform_keys::KeyPermissionsServiceFactory::GetForBrowserContext(
      context)
      ->IsCorporateKey(public_key_spki_der, std::move(callback));
}

// Returns true if the certificate is allowed to be used by ARC. The certificate
// is allowed to be used by ARC if its key is marked for corporate usage and
// resides on a user token. |cert| must be non-null.
void IsCertificateAllowed(IsCertificateAllowedCallback callback,
                          scoped_refptr<net::X509Certificate> cert,
                          content::BrowserContext* const context) {
  DCHECK(cert);

  const std::string public_key_spki_der =
      chromeos::platform_keys::GetSubjectPublicKeyInfo(cert);

  // Check if the key is on the user token.
  chromeos::platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
      context)
      ->IsKeyOnToken(
          chromeos::platform_keys::TokenId::kUser, public_key_spki_der,
          base::BindOnce(&CheckKeyLocationAndCorporateFlag, std::move(callback),
                         public_key_spki_der, context));
}

}  // namespace

// static
CertStoreService* CertStoreService::GetForBrowserContext(
    content::BrowserContext* context) {
  return CertStoreServiceFactory::GetForBrowserContext(context);
}

// static
BrowserContextKeyedServiceFactory* CertStoreService::GetFactory() {
  return CertStoreServiceFactory::GetInstance();
}

CertStoreService::CertStoreService(content::BrowserContext* context)
    : CertStoreService(context, std::make_unique<ArcCertInstaller>(context)) {}

CertStoreService::CertStoreService(content::BrowserContext* context,
                                   std::unique_ptr<ArcCertInstaller> installer)
    : context_(context), installer_(std::move(installer)) {
  // Do not perform any actions if context is nullptr for unit tests.
  if (!context_)
    return;

  net::CertDatabase::GetInstance()->AddObserver(this);

  UpdateCertificates();
}

CertStoreService::~CertStoreService() {
  if (context_)
    net::CertDatabase::GetInstance()->RemoveObserver(this);
}

void CertStoreService::OnCertDBChanged() {
  UpdateCertificates();
}

base::Optional<CertStoreService::KeyInfo>
CertStoreService::GetKeyInfoForDummySpki(const std::string& dummy_spki) {
  return certificate_cache_.GetKeyInfoForDummySpki(dummy_spki);
}

void CertStoreService::UpdateCertificates() {
  GetNSSCertDatabaseForProfile(
      Profile::FromBrowserContext(context_),
      base::BindOnce(&CertStoreService::OnGetNSSCertDatabaseForProfile,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CertStoreService::FilterAllowedCertificatesRecursively(
    FilterAllowedCertificatesCallback callback,
    base::queue<net::ScopedCERTCertificate> cert_queue,
    net::ScopedCERTCertificateList allowed_certs) const {
  if (cert_queue.empty()) {
    std::move(callback).Run(std::move(allowed_certs));
    return;
  }

  net::ScopedCERTCertificate cert = std::move(cert_queue.front());
  cert_queue.pop();

  scoped_refptr<net::X509Certificate> x509_cert =
      net::x509_util::CreateX509CertificateFromCERTCertificate(cert.get());

  if (!x509_cert) {
    FilterAllowedCertificatesRecursively(
        std::move(callback), std::move(cert_queue), std::move(allowed_certs));
    return;
  }

  IsCertificateAllowed(
      base::BindOnce(&CertStoreService::FilterAllowedCertificateAndRecurse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(cert_queue), std::move(allowed_certs),
                     std::move(cert)),
      std::move(x509_cert), context_);
}

void CertStoreService::FilterAllowedCertificateAndRecurse(
    FilterAllowedCertificatesCallback callback,
    base::queue<net::ScopedCERTCertificate> cert_queue,
    net::ScopedCERTCertificateList allowed_certs,
    net::ScopedCERTCertificate cert,
    bool certificate_allowed) const {
  if (certificate_allowed)
    allowed_certs.push_back(std::move(cert));

  FilterAllowedCertificatesRecursively(
      std::move(callback), std::move(cert_queue), std::move(allowed_certs));
}

void CertStoreService::OnGetNSSCertDatabaseForProfile(
    net::NSSCertDatabase* database) {
  DCHECK(database);
  database->ListCertsInSlot(
      base::BindOnce(&CertStoreService::OnCertificatesListed,
                     weak_ptr_factory_.GetWeakPtr()),
      database->GetPrivateSlot().get());
}

void CertStoreService::OnCertificatesListed(
    net::ScopedCERTCertificateList cert_list) {
  base::queue<net::ScopedCERTCertificate> cert_queue;
  for (auto& cert : cert_list) {
    cert_queue.push(std::move(cert));
  }

  net::ScopedCERTCertificateList allowed_certs;
  FilterAllowedCertificatesRecursively(
      base::BindOnce(&CertStoreService::OnFilteredAllowedCertificates,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(cert_queue), std::move(allowed_certs));
}

void CertStoreService::OnFilteredAllowedCertificates(
    net::ScopedCERTCertificateList allowed_certs) {
  certificate_cache_.clear_need_policy_update();
  auto certificates = certificate_cache_.Update(std::move(allowed_certs));

  // Maps cert name to dummy SPKI.
  std::map<std::string, std::string> installed_keys =
      installer_->InstallArcCerts(
          std::move(certificates),
          base::BindOnce(&CertStoreService::OnArcCertsInstalled,
                         weak_ptr_factory_.GetWeakPtr()));

  certificate_cache_.Update(installed_keys);
}

CertStoreService::CertificateCache::CertificateCache() = default;
CertStoreService::CertificateCache::~CertificateCache() = default;

net::ScopedCERTCertificateList CertStoreService::CertificateCache::Update(
    net::ScopedCERTCertificateList allowed_certs) {
  net::ScopedCERTCertificateList certs;
  // Map cert name to real SPKI.
  key_info_by_name_cache_.clear();
  std::set<std::string> new_required_cert_names;
  for (auto& cert : allowed_certs) {
    if (!cert)
      continue;
    std::string cert_name =
        x509_certificate_model::GetCertNameOrNickname(cert.get());
    SECKEYPrivateKey* priv_key =
        PK11_FindKeyByAnyCert(cert.get(), nullptr /* wincx */);
    if (!priv_key)
      continue;
    // Get the CKA_ID attribute for a key.
    SECItem* sec_item = PK11_GetLowLevelKeyIDForPrivateKey(priv_key);
    std::string pkcs11_id;
    if (sec_item) {
      pkcs11_id = base::HexEncode(sec_item->data, sec_item->len);
      SECITEM_FreeItem(sec_item, PR_TRUE);
    }
    SECKEY_DestroyPrivateKey(priv_key);

    key_info_by_name_cache_[cert_name] = {cert_name, pkcs11_id};
    new_required_cert_names.insert(cert_name);

    certs.push_back(std::move(cert));
  }
  need_policy_update_ = (required_cert_names_ != new_required_cert_names);
  for (auto cert_name : required_cert_names_) {
    if (!new_required_cert_names.count(cert_name)) {
      key_info_by_dummy_spki_cache_.erase(dummy_spki_by_name_cache_[cert_name]);
      dummy_spki_by_name_cache_.erase(cert_name);
    }
  }
  required_cert_names_ = new_required_cert_names;
  return certs;
}

void CertStoreService::CertificateCache::Update(
    std::map<std::string, std::string> dummy_spki_by_name) {
  if (required_cert_names_.size() != dummy_spki_by_name.size())
    return;
  for (const auto& cert : dummy_spki_by_name) {
    const std::string& name = cert.first;
    if (!required_cert_names_.count(name)) {
      VLOG(1) << "An attempt to add a non-required key " << name;
      continue;
    }

    std::string dummy_spki = cert.second;
    if (dummy_spki.empty() && dummy_spki_by_name_cache_.count(name))
      dummy_spki = dummy_spki_by_name_cache_[name];
    if (!dummy_spki.empty()) {
      dummy_spki_by_name_cache_[name] = dummy_spki;
      key_info_by_dummy_spki_cache_[dummy_spki] = key_info_by_name_cache_[name];
    }
  }
}

void CertStoreService::OnArcCertsInstalled(bool success) {
  VLOG(1) << "ARC certificates installation has finished with result="
          << success;
  if (certificate_cache_.need_policy_update()) {
    ArcPolicyBridge* const policy_bridge =
        ArcPolicyBridge::GetForBrowserContext(context_);
    if (policy_bridge) {
      policy_bridge->OnPolicyUpdated(policy::PolicyNamespace(),
                                     policy::PolicyMap(), policy::PolicyMap());
    }
  }
}

base::Optional<CertStoreService::KeyInfo>
CertStoreService::CertificateCache::GetKeyInfoForDummySpki(
    const std::string& dummy_spki) {
  if (key_info_by_dummy_spki_cache_.count(dummy_spki))
    return key_info_by_dummy_spki_cache_[dummy_spki];
  return base::nullopt;
}

}  // namespace arc
