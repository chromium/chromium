// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_cert_store_bridge.h"

#include <cert.h>

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/extension_key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_impl.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/cert_store.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_context.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"

namespace arc {

namespace {

// Singleton factory for ArcCertStoreBridge.
class ArcCertStoreBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcCertStoreBridge,
          ArcCertStoreBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcCertStoreBridgeFactory";

  static ArcCertStoreBridgeFactory* GetInstance() {
    return base::Singleton<ArcCertStoreBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcCertStoreBridgeFactory>;
  ArcCertStoreBridgeFactory() {}
  ~ArcCertStoreBridgeFactory() override = default;
};

bool LooksLikeAndroidPackageName(const std::string& name) {
  return name.find(".") != std::string::npos;
}

scoped_refptr<net::X509Certificate> FindCertificateByAlias(
    const std::string& alias) {
  // Searches certificate across all available slots without limiting to user's
  // private slot. It's OK as all certificates are filtered by the pref.
  net::ScopedCERTCertificate cert(
      CERT_FindCertByNickname(CERT_GetDefaultCertDB(), alias.c_str()));
  if (!cert) {
    LOG(ERROR) << "Certificate with nickname=" << alias << " does not exist.";
    return nullptr;
  }

  scoped_refptr<net::X509Certificate> x509_cert =
      net::x509_util::CreateX509CertificateFromCERTCertificate(cert.get());
  DVLOG_IF(1, !x509_cert)
      << "x509_util::CreateX509CertificateFromCERTCertificate failed";
  return x509_cert;
}

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
    std::move(callback).Run(/*allowed=*/false);
    return;
  }

  DCHECK(key_on_user_token.has_value());

  if (!key_on_user_token.value_or(false)) {
    std::move(callback).Run(/*allowed=*/false);
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
ArcCertStoreBridge* ArcCertStoreBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcCertStoreBridgeFactory::GetForBrowserContext(context);
}

ArcCertStoreBridge::ArcCertStoreBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : context_(context),
      arc_bridge_service_(bridge_service),
      weak_ptr_factory_(this) {
  DVLOG(1) << "ArcCertStoreBridge::ArcCertStoreBridge";

  const auto* profile_policy_connector =
      Profile::FromBrowserContext(context_)->GetProfilePolicyConnector();
  policy_service_ = profile_policy_connector->policy_service();
  DCHECK(policy_service_);

  arc_bridge_service_->cert_store()->SetHost(this);
  arc_bridge_service_->cert_store()->AddObserver(this);
}

ArcCertStoreBridge::~ArcCertStoreBridge() {
  DVLOG(1) << "ArcCertStoreBridge::~ArcCertStoreBridge";

  arc_bridge_service_->cert_store()->RemoveObserver(this);
  arc_bridge_service_->cert_store()->SetHost(nullptr);
}

void ArcCertStoreBridge::OnConnectionReady() {
  DVLOG(1) << "ArcCertStoreBridge::OnConnectionReady";

  policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
  net::CertDatabase::GetInstance()->AddObserver(this);

  UpdateFromKeyPermissionsPolicy();
}

void ArcCertStoreBridge::OnConnectionClosed() {
  DVLOG(1) << "ArcCertStoreBridge::OnConnectionClosed";

  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  net::CertDatabase::GetInstance()->RemoveObserver(this);
}

void ArcCertStoreBridge::ListCertificates(ListCertificatesCallback callback) {
  DVLOG(1) << "ArcCertStoreBridge::ListCertificates";

  if (!channel_enabled_) {
    DVLOG(1) << "ARC certificate store channel is not enabled.";
    std::move(callback).Run(std::vector<mojom::CertificatePtr>());
    return;
  }

  GetNSSCertDatabaseForProfile(
      Profile::FromBrowserContext(context_),
      base::Bind(&ArcCertStoreBridge::OnGetNSSCertDatabaseForProfile,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void ArcCertStoreBridge::GetKeyCharacteristics(
    const std::string& alias,
    GetKeyCharacteristicsCallback callback) {
  DVLOG(1) << "ArcCertStoreBridge::GetKeyCharacteristics";

  if (!channel_enabled_) {
    LOG(WARNING) << "ARC certificate store channel is not enabled.";
    std::move(callback).Run(mojom::KeymasterError::ERROR_INVALID_KEY_BLOB,
                            base::nullopt);
    return;
  }

  scoped_refptr<net::X509Certificate> cert = FindCertificateByAlias(alias);
  if (!cert) {
    std::move(callback).Run(mojom::KeymasterError::ERROR_INVALID_KEY_BLOB,
                            base::nullopt);
    return;
  }
  // TODO(pbond): implement.
  std::move(callback).Run(mojom::KeymasterError::ERROR_UNIMPLEMENTED,
                          base::nullopt);
}

void ArcCertStoreBridge::Begin(const std::string& alias,
                               const std::vector<mojom::KeyParamPtr> params,
                               BeginCallback callback) {
  DVLOG(1) << "ArcCertStoreBridge::Begin";

  if (!channel_enabled_) {
    LOG(WARNING) << "ARC certificate store channel is not enabled.";
    std::move(callback).Run(mojom::KeymasterError::ERROR_INVALID_KEY_BLOB, 0);
    return;
  }

  scoped_refptr<net::X509Certificate> cert = FindCertificateByAlias(alias);

  if (!cert) {
    std::move(callback).Run(mojom::KeymasterError::ERROR_INVALID_KEY_BLOB, 0);
    return;
  }
  // TODO(pbond): implement.
  std::move(callback).Run(mojom::KeymasterError::ERROR_UNIMPLEMENTED, 0);
}

void ArcCertStoreBridge::Update(uint64_t operation_handle,
                                const std::vector<uint8_t>& data,
                                UpdateCallback callback) {
  DVLOG(1) << "ArcCertStoreBridge::Update";

  if (!channel_enabled_) {
    LOG(WARNING) << "ARC certificate store channel is not enabled.";
    std::move(callback).Run(
        mojom::KeymasterError::ERROR_INVALID_OPERATION_HANDLE, 0);
    return;
  }
  // TODO(pbond): implement.
  std::move(callback).Run(mojom::KeymasterError::ERROR_UNIMPLEMENTED, 0);
}

void ArcCertStoreBridge::Finish(uint64_t operation_handle,
                                FinishCallback callback) {
  DVLOG(1) << "ArcCertStoreBridge::Finish";

  if (!channel_enabled_) {
    LOG(WARNING) << "ARC certificate store channel is not enabled.";
    std::move(callback).Run(
        mojom::KeymasterError::ERROR_INVALID_OPERATION_HANDLE, base::nullopt);
    return;
  }
  // TODO(pbond): implement.
  std::move(callback).Run(mojom::KeymasterError::ERROR_UNIMPLEMENTED,
                          base::nullopt);
}

void ArcCertStoreBridge::Abort(uint64_t operation_handle,
                               AbortCallback callback) {
  DVLOG(1) << "ArcCertStoreBridge::Abort";

  if (!channel_enabled_) {
    LOG(WARNING) << "ARC certificate store channel is not enabled.";
    std::move(callback).Run(
        mojom::KeymasterError::ERROR_INVALID_OPERATION_HANDLE);
    return;
  }
  // TODO(pbond): implement.
  std::move(callback).Run(mojom::KeymasterError::ERROR_UNIMPLEMENTED);
}

void ArcCertStoreBridge::OnCertDBChanged() {
  DVLOG(1) << "ArcCertStoreBridge::OnCertDBChanged";

  UpdateCertificates();
}

void ArcCertStoreBridge::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                         const policy::PolicyMap& previous,
                                         const policy::PolicyMap& current) {
  DVLOG(1) << "ArcCertStoreBridge::OnPolicyUpdated";

  const base::Value* previous_value =
      previous.GetValue(policy::key::kKeyPermissions);
  const base::Value* current_value =
      current.GetValue(policy::key::kKeyPermissions);

  // Check if KeyPermissions policy is changed.
  bool keyPermissionsPolicyUnchanged =
      ((previous_value == current_value) ||
       (previous_value && current_value && *previous_value == *current_value));
  if (!keyPermissionsPolicyUnchanged)
    UpdateFromKeyPermissionsPolicy();
}

void ArcCertStoreBridge::OnGetNSSCertDatabaseForProfile(
    ListCertificatesCallback callback,
    net::NSSCertDatabase* database) {
  DCHECK(database);
  // Lists certificates from both public and private slots. It's OK as all
  // certificates are filtered by prefs (corporate usage).
  // If filtering logic is changed, make sure certificates slots are correct.
  database->ListCerts(base::BindOnce(&ArcCertStoreBridge::OnCertificatesListed,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

void ArcCertStoreBridge::OnCertificatesListed(
    ListCertificatesCallback callback,
    net::ScopedCERTCertificateList cert_list) {
  base::queue<net::ScopedCERTCertificate> cert_queue;
  for (auto& cert : cert_list) {
    cert_queue.push(std::move(cert));
  }

  net::ScopedCERTCertificateList allowed_certs;

  FilterAllowedCertificatesRecursively(
      base::BindOnce(&ArcCertStoreBridge::OnFilteredAllowedCertificates,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      std::move(cert_queue), std::move(allowed_certs));
}

void ArcCertStoreBridge::FilterAllowedCertificatesRecursively(
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
      base::BindOnce(&ArcCertStoreBridge::FilterAllowedCertificateAndRecurse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(cert_queue), std::move(allowed_certs),
                     std::move(cert)),
      std::move(x509_cert), context_);
}

void ArcCertStoreBridge::FilterAllowedCertificateAndRecurse(
    FilterAllowedCertificatesCallback callback,
    base::queue<net::ScopedCERTCertificate> cert_queue,
    net::ScopedCERTCertificateList allowed_certs,
    net::ScopedCERTCertificate cert,
    bool certificate_allowed) const {
  if (certificate_allowed) {
    allowed_certs.push_back(std::move(cert));
  }

  FilterAllowedCertificatesRecursively(
      std::move(callback), std::move(cert_queue), std::move(allowed_certs));
}

void ArcCertStoreBridge::OnFilteredAllowedCertificates(
    ListCertificatesCallback callback,
    net::ScopedCERTCertificateList allowed_certs) {
  std::vector<mojom::CertificatePtr> certificate_ptr_list;

  for (const auto& cert : allowed_certs) {
    mojom::CertificatePtr certificate = mojom::Certificate::New();
    certificate->alias = cert->nickname;

    scoped_refptr<net::X509Certificate> x509_cert =
        net::x509_util::CreateX509CertificateFromCERTCertificate(cert.get());
    if (!x509_cert) {
      DVLOG(1) << "x509_util::CreateX509CertificateFromCERTCertificate failed";
      continue;
    }
    net::X509Certificate::GetPEMEncoded(x509_cert->cert_buffer(),
                                        &certificate->cert);
    certificate_ptr_list.emplace_back(std::move(certificate));
  }
  std::move(callback).Run(std::move(certificate_ptr_list));
}

void ArcCertStoreBridge::UpdateFromKeyPermissionsPolicy() {
  DVLOG(1) << "ArcCertStoreBridge::UpdateFromKeyPermissionsPolicy";

  std::vector<std::string> app_ids =
      chromeos::platform_keys::ExtensionKeyPermissionsService::
          GetCorporateKeyUsageAllowedAppIds(policy_service_);
  std::vector<std::string> permissions;
  for (const auto& app_id : app_ids) {
    if (LooksLikeAndroidPackageName(app_id))
      permissions.push_back(app_id);
  }

  channel_enabled_ = !permissions.empty();
  // If channel is enabled, the certificates must be updated.
  UpdateCertificates();

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->cert_store(), OnKeyPermissionsChanged);
  if (!instance)
    return;

  instance->OnKeyPermissionsChanged(std::move(permissions));
}

void ArcCertStoreBridge::UpdateCertificates() {
  DVLOG(1) << "ArcCertStoreBridge::UpdateCertificates";

  if (!channel_enabled_) {
    DVLOG(1) << "ARC certificate store channel is not enabled.";
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->cert_store(), OnCertificatesChanged);
  if (!instance)
    return;
  instance->OnCertificatesChanged();
}

}  // namespace arc
