// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_manager_model.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/ui/crypto_module_password_dialog_nss.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "crypto/nss_util.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#include "chromeos/network/onc/certificate_scope.h"
#include "chromeos/network/policy_certificate_provider.h"
#endif

using content::BrowserThread;

// CertificateManagerModel is created on the UI thread. It needs a
// NSSCertDatabase handle (and on ChromeOS it needs to get the TPM status) which
// needs to be done on the IO thread.
//
// The initialization flow is roughly:
//
//               UI thread                              IO Thread
//
//   CertificateManagerModel::Create
//                  \--------------------------------------v
//                                CertificateManagerModel::GetCertDBOnIOThread
//                                                         |
//                                     GetNSSCertDatabaseForResourceContext
//                                                         |
//                               CertificateManagerModel::DidGetCertDBOnIOThread
//                                                         |
//                                       crypto::IsTPMTokenEnabledForNSS
//                  v--------------------------------------/
// CertificateManagerModel::DidGetCertDBOnUIThread
//                  |
//     new CertificateManagerModel
//                  |
//               callback

namespace {

std::string GetCertificateOrg(CERTCertificate* cert) {
  std::string org =
      x509_certificate_model::GetSubjectOrgName(cert, std::string());
  if (org.empty())
    org = x509_certificate_model::GetSubjectDisplayName(cert);

  return org;
}

#if defined(OS_CHROMEOS)
// Log message for an operation that can not be performed on a certificate of a
// given source.
constexpr char kOperationNotPermitted[] =
    "Operation not permitted on a certificate. Source: ";
#endif  // OS_CHROMEOS

}  // namespace

// A source of certificates that should be displayed on the certificate manager
// UI. Currently, a CertsSource yields CertInfo objects. Each CertInfo contains
// a NSS ScopedCERTCertificate.
class CertificateManagerModel::CertsSource {
 public:
  // |certs_source_updated_callback| will be invoked when the list of
  // certificates provided by this CertsSource changes.
  explicit CertsSource(base::RepeatingClosure certs_source_updated_callback)
      : certs_source_updated_callback_(certs_source_updated_callback) {}
  virtual ~CertsSource() = default;

  // Returns the CertInfos provided by this CertsSource.
  const std::vector<std::unique_ptr<CertificateManagerModel::CertInfo>>&
  cert_infos() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return cert_infos_;
  }

  // Returns true if |cert| is in this CertsSource's certificate list.
  bool HasCert(CERTCertificate* cert) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (const auto& cert_info : cert_infos_) {
      if (cert_info->cert() == cert)
        return true;
    }
    return false;
  }

  // Triggers a refresh of this CertsSource. When done, the
  // |certs_source_updated_callback| passed to the constructor will be invoked.
  virtual void Refresh() = 0;

  // If any CertsSource's |IsHoldBackUpdates| is returning true, the
  // CertificateManagerModel will not notify its Observer about updates.
  bool IsHoldBackUpdates() const { return hold_back_updates_; }

  // Set trust values for certificate.
  // |trust_bits| should be a bit field of TRUST* values from NSSCertDatabase.
  // Returns true on success or false on failure.
  virtual bool SetCertTrust(CERTCertificate* cert,
                            net::CertType type,
                            net::NSSCertDatabase::TrustBits trust_bits) = 0;

  // Delete the cert. Returns true on success. |cert| is still valid when this
  // function returns.
  virtual bool Delete(CERTCertificate* cert) = 0;

 protected:
  // To be called by subclasses to set the CertInfo list provided by this
  // CertsSource. If this CertsSource is signalling that updates should be held
  // back (|SetHoldBackUpdates(true)|, this will be set to false. The
  // |certs_source_updated_callback| passed to the constructor will be invoked.
  void SetCertInfos(
      std::vector<std::unique_ptr<CertificateManagerModel::CertInfo>>
          cert_infos) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    cert_infos_.swap(cert_infos);
    SetHoldBackUpdates(false);
    certs_source_updated_callback_.Run();
  }

  // Signal to |CertificateManagerModel| that updates to its Observer should be
  // held back. This will be automatically taken back on |SetCertInfos|.
  // This should only be used by |CertsSource|s that provide their list of
  // certificates asynchronously but expect their certificate listing to be
  // fast.
  void SetHoldBackUpdates(bool hold_back_updates) {
    hold_back_updates_ = hold_back_updates;
  }

  // Used to verify that the constructor, and accessing |cert_infos_| are
  // performed on the same sequence. Offered to subclasses so they can also
  // check that they're being called on a valid sequence.
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Cached CertInfos provided by this CertsSource.
  std::vector<std::unique_ptr<CertificateManagerModel::CertInfo>> cert_infos_;

  // Invoked when the list of certificates provided by this CertsSource has
  // changed.
  base::RepeatingClosure certs_source_updated_callback_;

  // If true, the CertificateManagerModel should be holding back update
  // notifications.
  bool hold_back_updates_ = false;

  DISALLOW_COPY_AND_ASSIGN(CertsSource);
};

namespace {
// Provides certificates enumerable from a NSSCertDatabase.
class CertsSourcePlatformNSS : public CertificateManagerModel::CertsSource {
 public:
  CertsSourcePlatformNSS(base::RepeatingClosure certs_source_updated_callback,
                         net::NSSCertDatabase* nss_cert_database)
      : CertsSource(certs_source_updated_callback),
        cert_db_(nss_cert_database) {}
  ~CertsSourcePlatformNSS() override = default;

  void Refresh() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    SetHoldBackUpdates(true);
    DVLOG(1) << "refresh started";
    std::vector<crypto::ScopedPK11Slot> modules;
    cert_db_->ListModules(&modules, false);
    DVLOG(1) << "refresh waiting for unlocking...";
    chrome::UnlockSlotsIfNecessary(
        std::move(modules), kCryptoModulePasswordListCerts,
        net::HostPortPair(),  // unused.
        nullptr,              // TODO(mattm): supply parent window.
        base::BindOnce(&CertsSourcePlatformNSS::RefreshSlotsUnlocked,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  bool SetCertTrust(CERTCertificate* cert,
                    net::CertType type,
                    net::NSSCertDatabase::TrustBits trust_bits) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return cert_db_->SetCertTrust(cert, type, trust_bits);
  }

  bool Delete(CERTCertificate* cert) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool result = cert_db_->DeleteCertAndKey(cert);
    if (result)
      Refresh();
    return result;
  }

 private:
  void RefreshSlotsUnlocked() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DVLOG(1) << "refresh listing certs...";
    cert_db_->ListCerts(base::BindOnce(&CertsSourcePlatformNSS::DidGetCerts,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  void DidGetCerts(net::ScopedCERTCertificateList certs) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DVLOG(1) << "refresh finished for platform provided certificates";
    std::vector<std::unique_ptr<CertificateManagerModel::CertInfo>> cert_infos;

    cert_infos.reserve(certs.size());
    for (auto& cert : certs) {
      net::CertType type = x509_certificate_model::GetType(cert.get());
      bool can_be_deleted = !cert_db_->IsReadOnly(cert.get());
      bool untrusted = cert_db_->IsUntrusted(cert.get());
      bool hardware_backed = cert_db_->IsHardwareBacked(cert.get());
      bool web_trust_anchor = cert_db_->IsWebTrustAnchor(cert.get());
      bool device_wide = false;
#if defined(OS_CHROMEOS)
      device_wide = cert_db_->IsCertificateOnSystemSlot(cert.get());
#endif
      base::string16 name = GetName(cert.get(), hardware_backed);
      cert_infos.push_back(std::make_unique<CertificateManagerModel::CertInfo>(
          std::move(cert), type, name, can_be_deleted, untrusted,
          CertificateManagerModel::CertInfo::Source::kPlatform,
          web_trust_anchor, hardware_backed, device_wide));
    }

    SetCertInfos(std::move(cert_infos));
  }

  static base::string16 GetName(CERTCertificate* cert,
                                bool is_hardware_backed) {
    base::string16 name =
        base::UTF8ToUTF16(x509_certificate_model::GetCertNameOrNickname(cert));
    if (is_hardware_backed) {
      name = l10n_util::GetStringFUTF16(
          IDS_CERT_MANAGER_HARDWARE_BACKED_KEY_FORMAT, name,
          l10n_util::GetStringUTF16(IDS_CERT_MANAGER_HARDWARE_BACKED));
    }
    return name;
  }

  // The source NSSCertDatabase used for listing certificates.
  net::NSSCertDatabase* cert_db_;

  base::WeakPtrFactory<CertsSourcePlatformNSS> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CertsSourcePlatformNSS);
};

#if defined(OS_CHROMEOS)
// Provides certificates installed through enterprise policy.
class CertsSourcePolicy : public CertificateManagerModel::CertsSource,
                          chromeos::PolicyCertificateProvider::Observer {
 public:
  // Defines which policy-provided certificates this CertsSourcePolicy instance
  // should yield.
  enum class Mode {
    // Only certificates which are installed by enterprise policy, but not Web
    // trusted.
    kPolicyCertsWithoutWebTrust,
    // Only certificates which are installed by enterprise policy and Web
    // trusted.
    kPolicyCertsWithWebTrust
  };

  CertsSourcePolicy(base::RepeatingClosure certs_source_updated_callback,
                    chromeos::PolicyCertificateProvider* policy_certs_provider,
                    Mode mode)
      : CertsSource(certs_source_updated_callback),
        policy_certs_provider_(policy_certs_provider),
        mode_(mode) {
    policy_certs_provider_->AddPolicyProvidedCertsObserver(this);
  }

  ~CertsSourcePolicy() override {
    policy_certs_provider_->RemovePolicyProvidedCertsObserver(this);
  }

  // chromeos::PolicyCertificateProvider::Observer
  void OnPolicyProvidedCertsChanged() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Refresh();
  }

  void Refresh() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    switch (mode_) {
      case Mode::kPolicyCertsWithoutWebTrust:
        RefreshImpl(policy_certs_provider_->GetCertificatesWithoutWebTrust(
                        chromeos::onc::CertificateScope::Default()),
                    false /* policy_web_trusted */);
        break;
      case Mode::kPolicyCertsWithWebTrust:
        RefreshImpl(policy_certs_provider_->GetWebTrustedCertificates(
                        chromeos::onc::CertificateScope::Default()),
                    true /* policy_web_trusted */);
        break;
      default:
        NOTREACHED();
    }
  }

  bool SetCertTrust(CERTCertificate* cert,
                    net::CertType type,
                    net::NSSCertDatabase::TrustBits trust_bits) override {
    // Trust of policy-provided certificates can not be changed.
    LOG(WARNING) << kOperationNotPermitted << "Policy";
    return false;
  }

  bool Delete(CERTCertificate* cert) override {
    // Policy-provided certificates can not be deleted.
    LOG(WARNING) << kOperationNotPermitted << "Policy";
    return false;
  }

 private:
  void RefreshImpl(const net::CertificateList& certificates,
                   bool policy_web_trusted) {
    std::vector<std::unique_ptr<CertificateManagerModel::CertInfo>> cert_infos;
    cert_infos.reserve(certificates.size());

    for (const auto& policy_cert : certificates) {
      net::ScopedCERTCertificate nss_cert(
          net::x509_util::CreateCERTCertificateFromX509Certificate(
              policy_cert.get()));
      if (!nss_cert)
        continue;

      net::CertType type = x509_certificate_model::GetType(nss_cert.get());
      base::string16 cert_name = base::UTF8ToUTF16(
          x509_certificate_model::GetCertNameOrNickname(nss_cert.get()));
      cert_infos.push_back(std::make_unique<CertificateManagerModel::CertInfo>(
          std::move(nss_cert), type, std::move(cert_name),
          false /* can_be_deleted */, false /* untrusted */,
          CertificateManagerModel::CertInfo::Source::kPolicy,
          policy_web_trusted /* web_trust_anchor */,
          false /* hardware_backed */, false /* device_wide */));
    }

    SetCertInfos(std::move(cert_infos));
  }

  chromeos::PolicyCertificateProvider* policy_certs_provider_;
  Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(CertsSourcePolicy);
};

// Provides certificates made available by extensions through the
// chrome.certificateProvider API.
class CertsSourceExtensions : public CertificateManagerModel::CertsSource {
 public:
  CertsSourceExtensions(base::RepeatingClosure certs_source_updated_callback,
                        std::unique_ptr<chromeos::CertificateProvider>
                            certificate_provider_service)
      : CertsSource(certs_source_updated_callback),
        certificate_provider_service_(std::move(certificate_provider_service)) {
  }

  void Refresh() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    certificate_provider_service_->GetCertificates(base::BindOnce(
        &CertsSourceExtensions::DidGetCerts, weak_ptr_factory_.GetWeakPtr()));
  }

  bool SetCertTrust(CERTCertificate* cert,
                    net::CertType type,
                    net::NSSCertDatabase::TrustBits trust_bits) override {
    // Extension-provided certificates are user certificates; changing trust
    // does not make sense here.
    LOG(WARNING) << kOperationNotPermitted << "Extension";
    return false;
  }

  bool Delete(CERTCertificate* cert) override {
    // Extension-provided certificates can not be deleted.
    LOG(WARNING) << kOperationNotPermitted << "Extension";
    return false;
  }

 private:
  void DidGetCerts(net::ClientCertIdentityList cert_identities) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::vector<std::unique_ptr<CertificateManagerModel::CertInfo>> cert_infos;

    cert_infos.reserve(cert_identities.size());
    for (const auto& identity : cert_identities) {
      net::ScopedCERTCertificate nss_cert(
          net::x509_util::CreateCERTCertificateFromX509Certificate(
              identity->certificate()));
      if (!nss_cert)
        continue;

      base::string16 cert_name = base::UTF8ToUTF16(
          x509_certificate_model::GetCertNameOrNickname(nss_cert.get()));
      base::string16 display_name = l10n_util::GetStringFUTF16(
          IDS_CERT_MANAGER_EXTENSION_PROVIDED_FORMAT, std::move(cert_name));

      cert_infos.push_back(std::make_unique<CertificateManagerModel::CertInfo>(
          std::move(nss_cert), net::CertType::USER_CERT /* type */,
          display_name, false /* can_be_deleted */, false /* untrusted */,
          CertificateManagerModel::CertInfo::Source::kExtension,
          false /* web_trust_anchor */, false /* hardware_backed */,
          false /* device_wide */));
    }

    SetCertInfos(std::move(cert_infos));
  }

  std::unique_ptr<chromeos::CertificateProvider> certificate_provider_service_;

  base::WeakPtrFactory<CertsSourceExtensions> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CertsSourceExtensions);
};

#endif  // OS_CHROMEOS

}  // namespace

CertificateManagerModel::CertInfo::CertInfo(net::ScopedCERTCertificate cert,
                                            net::CertType type,
                                            base::string16 name,
                                            bool can_be_deleted,
                                            bool untrusted,
                                            Source source,
                                            bool web_trust_anchor,
                                            bool hardware_backed,
                                            bool device_wide)
    : cert_(std::move(cert)),
      type_(type),
      name_(std::move(name)),
      can_be_deleted_(can_be_deleted),
      untrusted_(untrusted),
      source_(source),
      web_trust_anchor_(web_trust_anchor),
      hardware_backed_(hardware_backed),
      device_wide_(device_wide) {}

CertificateManagerModel::CertInfo::~CertInfo() {}

// static
std::unique_ptr<CertificateManagerModel::CertInfo>
CertificateManagerModel::CertInfo::Clone(const CertInfo* cert_info) {
  return std::make_unique<CertInfo>(
      net::x509_util::DupCERTCertificate(cert_info->cert()), cert_info->type(),
      cert_info->name(), cert_info->can_be_deleted(), cert_info->untrusted(),
      cert_info->source(), cert_info->web_trust_anchor(),
      cert_info->hardware_backed(), cert_info->device_wide());
}

CertificateManagerModel::Params::Params() = default;
CertificateManagerModel::Params::~Params() = default;
CertificateManagerModel::Params::Params(Params&& other) = default;

// static
void CertificateManagerModel::Create(
    content::BrowserContext* browser_context,
    CertificateManagerModel::Observer* observer,
    const CreationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<Params> params = std::make_unique<Params>();
#if defined(OS_CHROMEOS)
  policy::UserNetworkConfigurationUpdater* user_network_configuration_updater =
      policy::UserNetworkConfigurationUpdaterFactory::GetForBrowserContext(
          browser_context);
  params->policy_certs_provider = user_network_configuration_updater;

  chromeos::CertificateProviderService* certificate_provider_service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context);
  params->extension_certificate_provider =
      certificate_provider_service->CreateCertificateProvider();
#endif

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CertificateManagerModel::GetCertDBOnIOThread,
                     std::move(params), browser_context->GetResourceContext(),
                     observer, callback));
}

CertificateManagerModel::CertificateManagerModel(
    std::unique_ptr<Params> params,
    Observer* observer,
    net::NSSCertDatabase* nss_cert_database,
    bool is_user_db_available,
    bool is_tpm_available)
    : cert_db_(nss_cert_database),
      is_user_db_available_(is_user_db_available),
      is_tpm_available_(is_tpm_available),
      observer_(observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Fill |certs_sources_|. Note that the order matters. Higher priority
  // CertsSources must come first.

  base::RepeatingClosure certs_source_updated_callback = base::BindRepeating(
      &CertificateManagerModel::OnCertsSourceUpdated, base::Unretained(this));

#if defined(OS_CHROMEOS)
  // Certificates installed and web trusted by enterprise policy is the highest
  // priority CertsSource.
  // UserNetworkConfigurationUpdater is only available for the primary user's
  // profile.
  if (params->policy_certs_provider) {
    certs_sources_.push_back(std::make_unique<CertsSourcePolicy>(
        certs_source_updated_callback, params->policy_certs_provider,
        CertsSourcePolicy::Mode::kPolicyCertsWithWebTrust));
  }
#endif

  // Add the main NSS DB based CertsSource.
  certs_sources_.push_back(std::make_unique<CertsSourcePlatformNSS>(
      certs_source_updated_callback, nss_cert_database));

#if defined(OS_CHROMEOS)
  // Certificates installed by enterprise policy without web trust are lower
  // priority than the main NSS DB based CertsSource.
  // Rationale: The user should be able to add trust to policy-provided
  // certificates by re-importing them and modifying their trust settings.
  if (params->policy_certs_provider) {
    certs_sources_.push_back(std::make_unique<CertsSourcePolicy>(
        certs_source_updated_callback, params->policy_certs_provider,
        CertsSourcePolicy::Mode::kPolicyCertsWithoutWebTrust));
  }

  // Extensions is the lowest priority CertsSource.
  if (params->extension_certificate_provider) {
    certs_sources_.push_back(std::make_unique<CertsSourceExtensions>(
        certs_source_updated_callback,
        std::move(params->extension_certificate_provider)));
  }
#endif
}

CertificateManagerModel::~CertificateManagerModel() {}

void CertificateManagerModel::OnCertsSourceUpdated() {
  if (hold_back_updates_)
    return;
  for (const auto& certs_source : certs_sources_) {
    if (certs_source->IsHoldBackUpdates()) {
      return;
    }
  }

  observer_->CertificatesRefreshed();
}

CertificateManagerModel::CertsSource*
CertificateManagerModel::FindCertsSourceForCert(CERTCertificate* cert) {
  for (auto& certs_source : certs_sources_) {
    if (certs_source->HasCert(cert))
      return certs_source.get();
  }
  return nullptr;
}

void CertificateManagerModel::Refresh() {
  hold_back_updates_ = true;

  for (auto& certs_source : certs_sources_)
    certs_source->Refresh();

  hold_back_updates_ = false;
  OnCertsSourceUpdated();
}

void CertificateManagerModel::FilterAndBuildOrgGroupingMap(
    net::CertType filter_type,
    CertificateManagerModel::OrgGroupingMap* out_org_grouping_map) const {
  std::map<CERTCertificate*, std::unique_ptr<CertInfo>> cert_info_map;
  for (const auto& certs_source : certs_sources_) {
    for (const auto& cert_info : certs_source->cert_infos()) {
      if (cert_info->type() != filter_type)
        continue;

      if (cert_info_map.find(cert_info->cert()) == cert_info_map.end())
        cert_info_map[cert_info->cert()] = CertInfo::Clone(cert_info.get());
    }
  }

  for (auto& cert_info_kv : cert_info_map) {
    std::string org = GetCertificateOrg(cert_info_kv.second->cert());
    (*out_org_grouping_map)[org].push_back(std::move(cert_info_kv.second));
  }
}

int CertificateManagerModel::ImportFromPKCS12(PK11SlotInfo* slot_info,
                                              const std::string& data,
                                              const base::string16& password,
                                              bool is_extractable) {
  int result = cert_db_->ImportFromPKCS12(slot_info, data, password,
                                          is_extractable, nullptr);
  if (result == net::OK)
    Refresh();
  return result;
}

int CertificateManagerModel::ImportUserCert(const std::string& data) {
  int result = cert_db_->ImportUserCert(data);
  if (result == net::OK)
    Refresh();
  return result;
}

bool CertificateManagerModel::ImportCACerts(
    const net::ScopedCERTCertificateList& certificates,
    net::NSSCertDatabase::TrustBits trust_bits,
    net::NSSCertDatabase::ImportCertFailureList* not_imported) {
  const size_t num_certs = certificates.size();
  bool result = cert_db_->ImportCACerts(certificates, trust_bits, not_imported);
  if (result && not_imported->size() != num_certs)
    Refresh();
  return result;
}

bool CertificateManagerModel::ImportServerCert(
    const net::ScopedCERTCertificateList& certificates,
    net::NSSCertDatabase::TrustBits trust_bits,
    net::NSSCertDatabase::ImportCertFailureList* not_imported) {
  const size_t num_certs = certificates.size();
  bool result =
      cert_db_->ImportServerCert(certificates, trust_bits, not_imported);
  if (result && not_imported->size() != num_certs)
    Refresh();
  return result;
}

bool CertificateManagerModel::SetCertTrust(
    CERTCertificate* cert,
    net::CertType type,
    net::NSSCertDatabase::TrustBits trust_bits) {
  CertsSource* certs_source = FindCertsSourceForCert(cert);
  if (!certs_source)
    return false;
  return certs_source->SetCertTrust(cert, type, trust_bits);
}

bool CertificateManagerModel::Delete(CERTCertificate* cert) {
  CertsSource* certs_source = FindCertsSourceForCert(cert);
  if (!certs_source)
    return false;
  return certs_source->Delete(cert);
}

// static
void CertificateManagerModel::DidGetCertDBOnUIThread(
    std::unique_ptr<Params> params,
    CertificateManagerModel::Observer* observer,
    const CreationCallback& callback,
    net::NSSCertDatabase* cert_db,
    bool is_user_db_available,
    bool is_tpm_available) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<CertificateManagerModel> model =
      std::make_unique<CertificateManagerModel>(std::move(params), observer,
                                                cert_db, is_user_db_available,
                                                is_tpm_available);
  callback.Run(std::move(model));
}

// static
void CertificateManagerModel::DidGetCertDBOnIOThread(
    std::unique_ptr<Params> params,
    CertificateManagerModel::Observer* observer,
    const CreationCallback& callback,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bool is_user_db_available = !!cert_db->GetPublicSlot();
  bool is_tpm_available = false;
#if defined(OS_CHROMEOS)
  is_tpm_available = crypto::IsTPMTokenEnabledForNSS();
#endif
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CertificateManagerModel::DidGetCertDBOnUIThread,
                     std::move(params), observer, callback, cert_db,
                     is_user_db_available, is_tpm_available));
}

// static
void CertificateManagerModel::GetCertDBOnIOThread(
    std::unique_ptr<Params> params,
    content::ResourceContext* resource_context,
    CertificateManagerModel::Observer* observer,
    const CreationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto did_get_cert_db_callback = base::AdaptCallbackForRepeating(
      base::BindOnce(&CertificateManagerModel::DidGetCertDBOnIOThread,
                     std::move(params), observer, callback));

  net::NSSCertDatabase* cert_db = GetNSSCertDatabaseForResourceContext(
      resource_context, did_get_cert_db_callback);
  // If the NSS database was already available, |cert_db| is non-null and
  // |did_get_cert_db_callback| has not been called. Call it explicitly.
  if (cert_db)
    did_get_cert_db_callback.Run(cert_db);
}
