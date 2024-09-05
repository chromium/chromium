// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_manager_model.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/crypto_module_password_dialog_nss.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/net_buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_histograms.h"
#include "chrome/browser/ash/kcer/kcer_factory_ash.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_ash.h"
#include "chromeos/components/onc/certificate_scope.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
//                                               NssCertDatabaseGetter
//                                                         |
//                               CertificateManagerModel::DidGetCertDBOnIOThread
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

#if BUILDFLAG(IS_CHROMEOS)
// Log message for an operation that can not be performed on a certificate of a
// given source.
constexpr char kOperationNotPermitted[] =
    "Operation not permitted on a certificate. Source: ";
#endif  // BUILDFLAG(IS_CHROMEOS)

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

  CertsSource(const CertsSource&) = delete;
  CertsSource& operator=(const CertsSource&) = delete;

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

  // Remove the cert from the cert database.
  virtual void RemoveFromDatabase(
      net::ScopedCERTCertificate cert,
      base::OnceCallback<void(bool /*success*/)> callback) = 0;

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
};

namespace {
// Provides certificates enumerable from a NSSCertDatabase.
class CertsSourcePlatformNSS : public CertificateManagerModel::CertsSource,
                               net::CertDatabase::Observer {
 public:
  CertsSourcePlatformNSS(base::RepeatingClosure certs_source_updated_callback,
                         net::NSSCertDatabase* nss_cert_database)
      : CertsSource(certs_source_updated_callback),
        cert_db_(nss_cert_database) {
    // Observe CertDatabase changes to refresh when it's updated.
    cert_database_observation_.Observe(net::CertDatabase::GetInstance());
  }

  CertsSourcePlatformNSS(const CertsSourcePlatformNSS&) = delete;
  CertsSourcePlatformNSS& operator=(const CertsSourcePlatformNSS&) = delete;

  ~CertsSourcePlatformNSS() override = default;

  // net::CertDatabase::Observer
  void OnTrustStoreChanged() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Refresh();
  }
  void OnClientCertStoreChanged() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Refresh();
  }

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

  void RemoveFromDatabase(net::ScopedCERTCertificate cert,
                          base::OnceCallback<void(bool)> callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto callback_and_runner =
        base::BindPostTaskToCurrentDefault(std::move(callback));

    // Passing Unretained(cert_db_) is safe because the corresponding profile
    // should be alive during this call and therefore the deletion task for the
    // database can only be scheduled on the IO thread after this task.
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&net::NSSCertDatabase::DeleteCertAndKeyAsync,
                                  base::Unretained(cert_db_), std::move(cert),
                                  std::move(callback_and_runner)));
  }

 private:
  void RefreshSlotsUnlocked() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DVLOG(1) << "refresh listing certs...";
    cert_db_->ListCertsInfo(base::BindOnce(&CertsSourcePlatformNSS::DidGetCerts,
                                           weak_ptr_factory_.GetWeakPtr()),
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
                            net::NSSCertDatabase::NSSRootsHandling::kExclude
#else
                            net::NSSCertDatabase::NSSRootsHandling::kInclude
#endif
    );
  }

  void DidGetCerts(net::NSSCertDatabase::CertInfoList cert_info_list) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DVLOG(1) << "refresh finished for platform provided certificates";

    std::vector<std::unique_ptr<CertificateManagerModel::CertInfo>> cert_infos;
    cert_infos.reserve(cert_info_list.size());

    for (auto& cert_info : cert_info_list) {
      net::CertType type =
          x509_certificate_model::GetType(cert_info.cert.get());
      bool can_be_deleted = !cert_info.on_read_only_slot;
      bool hardware_backed = cert_info.hardware_backed;
      std::u16string name = GetName(cert_info.cert.get(), hardware_backed);

      cert_infos.push_back(std::make_unique<CertificateManagerModel::CertInfo>(
          /*cert=*/std::move(cert_info.cert), type, name, can_be_deleted,
          /*untrusted=*/cert_info.untrusted,
          /*source=*/CertificateManagerModel::CertInfo::Source::kPlatform,
          /*web_trust_anchor=*/cert_info.web_trust_anchor, hardware_backed,
          /*device_wide=*/cert_info.device_wide));
    }

    SetCertInfos(std::move(cert_infos));
  }

  static std::u16string GetName(CERTCertificate* cert,
                                bool is_hardware_backed) {
    std::u16string name =
        base::UTF8ToUTF16(x509_certificate_model::GetCertNameOrNickname(cert));
    if (is_hardware_backed) {
      name = l10n_util::GetStringFUTF16(
          IDS_CERT_MANAGER_HARDWARE_BACKED_KEY_FORMAT, name,
          l10n_util::GetStringUTF16(IDS_CERT_MANAGER_HARDWARE_BACKED));
    }
    return name;
  }

  // The source NSSCertDatabase used for listing certificates.
  raw_ptr<net::NSSCertDatabase> cert_db_;

  // ScopedObservation to keep track of the observer for net::CertDatabase.
  base::ScopedObservation<net::CertDatabase, net::CertDatabase::Observer>
      cert_database_observation_{this};

  base::WeakPtrFactory<CertsSourcePlatformNSS> weak_ptr_factory_{this};
};

#if BUILDFLAG(IS_CHROMEOS)
// Provides certificates installed through enterprise policy.
class CertsSourcePolicy : public CertificateManagerModel::CertsSource,
                          ash::PolicyCertificateProvider::Observer {
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
                    ash::PolicyCertificateProvider* policy_certs_provider,
                    Mode mode)
      : CertsSource(certs_source_updated_callback),
        policy_certs_provider_(policy_certs_provider),
        mode_(mode) {
    policy_certs_provider_->AddPolicyProvidedCertsObserver(this);
  }

  CertsSourcePolicy(const CertsSourcePolicy&) = delete;
  CertsSourcePolicy& operator=(const CertsSourcePolicy&) = delete;

  ~CertsSourcePolicy() override {
    policy_certs_provider_->RemovePolicyProvidedCertsObserver(this);
  }

  // ash::PolicyCertificateProvider::Observer
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
        NOTREACHED_IN_MIGRATION();
    }
  }

  bool SetCertTrust(CERTCertificate* cert,
                    net::CertType type,
                    net::NSSCertDatabase::TrustBits trust_bits) override {
    // Trust of policy-provided certificates can not be changed.
    LOG(WARNING) << kOperationNotPermitted << "Policy";
    return false;
  }

  void RemoveFromDatabase(net::ScopedCERTCertificate cert,
                          base::OnceCallback<void(bool)> callback) override {
    // Policy-provided certificates can not be deleted.
    LOG(WARNING) << kOperationNotPermitted << "Policy";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
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
      std::u16string cert_name = base::UTF8ToUTF16(
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

  raw_ptr<ash::PolicyCertificateProvider> policy_certs_provider_;
  Mode mode_;
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

  CertsSourceExtensions(const CertsSourceExtensions&) = delete;
  CertsSourceExtensions& operator=(const CertsSourceExtensions&) = delete;

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

  void RemoveFromDatabase(net::ScopedCERTCertificate cert,
                          base::OnceCallback<void(bool)> callback) override {
    // Extension-provided certificates can not be deleted.
    LOG(WARNING) << kOperationNotPermitted << "Extension";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
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

      std::u16string cert_name = base::UTF8ToUTF16(
          x509_certificate_model::GetCertNameOrNickname(nss_cert.get()));
      std::u16string display_name = l10n_util::GetStringFUTF16(
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
};

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)

void RecordImportFromPKCS12KcerResult(
    int nss_import_result,
    base::OnceCallback<void(int nss_import_result)> callback,
    base::expected<void, kcer::Error> kcer_import_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (kcer_import_result.has_value()) {
    kcer::RecordPkcs12MigrationUmaEvent(
        kcer::Pkcs12MigrationUmaEvent::kPkcs12ImportKcerSuccess);
  } else {
    kcer::RecordPkcs12MigrationUmaEvent(
        kcer::Pkcs12MigrationUmaEvent::kPkcs12ImportKcerFailed);
    kcer::RecordKcerError(kcer_import_result.error());
  }

  // Just return the nss_import_result. Kcer will attempt to import only if NSS
  // succeeds and even if Kcer fails, the cert should be usable.
  return std::move(callback).Run(nss_import_result);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

CertificateManagerModel::CertInfo::CertInfo(net::ScopedCERTCertificate cert,
                                            net::CertType type,
                                            std::u16string name,
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
    CreationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<Params> params = std::make_unique<Params>();
#if BUILDFLAG(IS_CHROMEOS)
  params->policy_certs_provider =
      policy::UserNetworkConfigurationUpdaterFactory::GetForBrowserContext(
          browser_context);

  chromeos::CertificateProviderService* certificate_provider_service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context);
  params->extension_certificate_provider =
      certificate_provider_service->CreateCertificateProvider();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  params->kcer = kcer::KcerFactoryAsh::GetKcer(
      Profile::FromBrowserContext(browser_context));
#endif

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CertificateManagerModel::GetCertDBOnIOThread,
                     std::move(params),
                     NssServiceFactory::GetForContext(browser_context)
                         ->CreateNSSCertDatabaseGetterForIOThread(),
                     observer, std::move(callback)));
}

CertificateManagerModel::CertificateManagerModel(
    std::unique_ptr<Params> params,
    Observer* observer,
    net::NSSCertDatabase* nss_cert_database)
    : cert_db_(nss_cert_database), observer_(observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Fill |certs_sources_|. Note that the order matters. Higher priority
  // CertsSources must come first.

  base::RepeatingClosure certs_source_updated_callback = base::BindRepeating(
      &CertificateManagerModel::OnCertsSourceUpdated, base::Unretained(this));

#if BUILDFLAG(IS_CHROMEOS)
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  kcer_ = params->kcer;
#endif

  // Add the main NSS DB based CertsSource.
  certs_sources_.push_back(std::make_unique<CertsSourcePlatformNSS>(
      certs_source_updated_callback, nss_cert_database));

#if BUILDFLAG(IS_CHROMEOS)
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

void CertificateManagerModel::ImportFromPKCS12(
    PK11SlotInfo* slot_info,
    const std::string& data,
    const std::u16string& password,
    bool is_extractable,
    base::OnceCallback<void(int nss_import_result)> callback) {
  int nss_import_result = cert_db_->ImportFromPKCS12(slot_info, data, password,
                                                     is_extractable, nullptr);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (nss_import_result == net::OK) {
    kcer::RecordPkcs12MigrationUmaEvent(
        kcer::Pkcs12MigrationUmaEvent::kPkcs12ImportNssSuccess);
  } else {
    kcer::RecordPkcs12MigrationUmaEvent(
        kcer::Pkcs12MigrationUmaEvent::kPkcs12ImportNssFailed);
  }

  // `is_extractable` == true indicates that the cert came from the "Import"
  // button. By default it's imported into the software NSS database (aka public
  // slot). With the experiment enabled it should also be imported into Chaps.
  // `is_extractable` == false means that the cert came from the "Import and
  // Bind" button and it's import into Chaps by default.
  if ((nss_import_result == net::OK) && is_extractable &&
      chromeos::features::IsPkcs12ToChapsDualWriteEnabled()) {
    // Record the dual-write event. Even if the import fails, it's theoretically
    // possible that some related objects are still created and would need to be
    // deleted in case of a rollback.
    kcer::KcerFactoryAsh::RecordPkcs12CertDualWritten();
    std::string u8_password = base::UTF16ToUTF8(password);
    return kcer_->ImportPkcs12Cert(
        kcer::Token::kUser,
        kcer::Pkcs12Blob(std::vector<uint8_t>(data.begin(), data.end())),
        std::move(u8_password),
        /*hardware_backed=*/!is_extractable, /*mark_as_migrated=*/true,
        base::BindOnce(&RecordImportFromPKCS12KcerResult, nss_import_result,
                       std::move(callback)));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return std::move(callback).Run(nss_import_result);
}

int CertificateManagerModel::ImportUserCert(const std::string& data) {
  return cert_db_->ImportUserCert(data);
}

bool CertificateManagerModel::ImportCACerts(
    const net::ScopedCERTCertificateList& certificates,
    net::NSSCertDatabase::TrustBits trust_bits,
    net::NSSCertDatabase::ImportCertFailureList* not_imported) {
  return cert_db_->ImportCACerts(certificates, trust_bits, not_imported);
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

void CertificateManagerModel::RemoveFromDatabase(
    net::ScopedCERTCertificate cert,
    base::OnceCallback<void(bool)> callback) {
  CertsSource* certs_source = FindCertsSourceForCert(cert.get());
  if (!certs_source) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  return certs_source->RemoveFromDatabase(std::move(cert), std::move(callback));
}

// static
void CertificateManagerModel::DidGetCertDBOnUIThread(
    std::unique_ptr<Params> params,
    CertificateManagerModel::Observer* observer,
    CreationCallback callback,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<CertificateManagerModel> model =
      std::make_unique<CertificateManagerModel>(std::move(params), observer,
                                                cert_db);
  std::move(callback).Run(std::move(model));
}

// static
void CertificateManagerModel::DidGetCertDBOnIOThread(
    std::unique_ptr<Params> params,
    CertificateManagerModel::Observer* observer,
    CreationCallback callback,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CertificateManagerModel::DidGetCertDBOnUIThread,
                     std::move(params), observer, std::move(callback),
                     cert_db));
}

// static
void CertificateManagerModel::GetCertDBOnIOThread(
    std::unique_ptr<Params> params,
    NssCertDatabaseGetter database_getter,
    CertificateManagerModel::Observer* observer,
    CreationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&CertificateManagerModel::DidGetCertDBOnIOThread,
                     std::move(params), observer, std::move(callback)));

  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(std::move(split_callback.first));
  // If the NSS database was already available, |cert_db| is non-null and
  // |did_get_cert_db_callback| has not been called. Call it explicitly.
  if (cert_db)
    std::move(split_callback.second).Run(cert_db);
}
