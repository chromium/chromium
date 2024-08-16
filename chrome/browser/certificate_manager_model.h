// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_MANAGER_MODEL_H_
#define CHROME_BROWSER_CERTIFICATE_MANAGER_MODEL_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/nss_service.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/ssl/client_cert_identity.h"

namespace content {
class BrowserContext;
}  // namespace content

#if BUILDFLAG(IS_CHROMEOS)
namespace ash {
class PolicyCertificateProvider;
}

namespace chromeos {
class CertificateProvider;
}

namespace kcer {
class Kcer;
}
#endif

// CertificateManagerModel provides the data to be displayed in the certificate
// manager dialog, and processes changes from the view.
class CertificateManagerModel {
 public:
  // Holds information about a certificate, along with the certificate itself.
  class CertInfo {
   public:
    enum class Source {
      // This certificate is installed in the platform certificate database.
      kPlatform,
      // This certificate is provided by enterprise policy.
      kPolicy,
      // This certificate is provided by an extension.
      kExtension
    };

    CertInfo(net::ScopedCERTCertificate cert,
             net::CertType type,
             std::u16string name,
             bool can_be_deleted,
             bool untrusted,
             Source source,
             bool web_trust_anchor,
             bool hardware_backed,
             bool device_wide);

    CertInfo(const CertInfo&) = delete;
    CertInfo& operator=(const CertInfo&) = delete;

    ~CertInfo();

    CERTCertificate* cert() const { return cert_.get(); }
    net::CertType type() const { return type_; }
    const std::u16string& name() const { return name_; }
    bool can_be_deleted() const { return can_be_deleted_; }
    bool untrusted() const { return untrusted_; }
    Source source() const { return source_; }
    bool web_trust_anchor() const { return web_trust_anchor_; }
    bool hardware_backed() const { return hardware_backed_; }
    bool device_wide() const { return device_wide_; }

    // Clones a CertInfo, duplicating the contained NSS certificate.
    static std::unique_ptr<CertInfo> Clone(const CertInfo* cert_info);

   private:
    // The certificate itself.
    net::ScopedCERTCertificate cert_;

    // The type of the certificate. Used to filter certificates to be displayed
    // on the tabs of the certificate manager UI.
    net::CertType type_;

    // A user readable certificate name.
    std::u16string name_;

    // false if the certificate is stored on a read-only slot or provided by
    // enterprise policy or an extension, otherwise true.
    bool can_be_deleted_;

    // true if the certificate is untrusted.
    bool untrusted_;

    // Describes where this certificate originates from.
    Source source_;

    // true if the certificate is given web trust (either by its platform trust
    // settings, or by enterprise policy).
    bool web_trust_anchor_;

    // true if the certificate is hardware-backed. Note that extension-provided
    // certificates are not regarded as hardware-backed.
    bool hardware_backed_;

    // true if the certificate is device-wide.
    // Note: can be true only on Chrome OS.
    bool device_wide_;

    FRIEND_TEST_ALL_PREFIXES(CertificateHandlerTest,
                             CanDeleteCertificateCommonTest);
    FRIEND_TEST_ALL_PREFIXES(CertificateHandlerTest,
                             CanDeleteUserCertificateTest);
    FRIEND_TEST_ALL_PREFIXES(CertificateHandlerTest,
                             CanDeleteCACertificateTest);
    FRIEND_TEST_ALL_PREFIXES(CertificateHandlerTest,
                             CanEditCertificateCommonTest);
    FRIEND_TEST_ALL_PREFIXES(CertificateHandlerTest,
                             CanEditUserCertificateTest);
    FRIEND_TEST_ALL_PREFIXES(CertificateHandlerTest, CanEditCACertificateTest);
  };

  class CertsSource;

  // Holds parameters during construction.
  struct Params {
#if BUILDFLAG(IS_CHROMEOS)
    // May be nullptr.
    raw_ptr<ash::PolicyCertificateProvider> policy_certs_provider = nullptr;
    // May be nullptr.
    std::unique_ptr<chromeos::CertificateProvider>
        extension_certificate_provider;
    // Valid as long as the underlying Profile is valid. The implementation
    // doesn't check for validity of the WeakPtr because the
    // CertificateManagerModel has the same validity time frame.
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    base::WeakPtr<kcer::Kcer> kcer;
#endif

    Params();

    Params(const Params&) = delete;
    Params& operator=(const Params&) = delete;

    Params(Params&& other);

    ~Params();
  };

  // Map from the subject organization name to the list of certs from that
  // organization.  If a cert does not have an organization name, the
  // subject's CertPrincipal::GetDisplayName() value is used instead.
  using OrgGroupingMap =
      std::map<std::string, std::vector<std::unique_ptr<CertInfo>>>;

  using CreationCallback =
      base::OnceCallback<void(std::unique_ptr<CertificateManagerModel>)>;

  class Observer {
   public:
    // Called to notify the view that the certificate list has been refreshed.
    // TODO(mattm): do a more granular updating strategy?  Maybe retrieve new
    // list of certs, diff against past list, and then notify of the changes?
    virtual void CertificatesRefreshed() = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Creates a CertificateManagerModel. The model will be passed to the callback
  // when it is ready. The caller must ensure the model does not outlive the
  // |browser_context|.
  static void Create(content::BrowserContext* browser_context,
                     Observer* observer,
                     CreationCallback callback);

  // Use |Create| instead to create a |CertificateManagerModel| for a
  // |BrowserContext|.
  CertificateManagerModel(std::unique_ptr<Params> params,
                          Observer* observer,
                          net::NSSCertDatabase* nss_cert_database);

  CertificateManagerModel(const CertificateManagerModel&) = delete;
  CertificateManagerModel& operator=(const CertificateManagerModel&) = delete;

  ~CertificateManagerModel();

  // Accessor for read-only access to the underlying NSSCertDatabase.
  const net::NSSCertDatabase* cert_db() const { return cert_db_; }

  // Trigger a refresh of the list of certs, unlock any slots if necessary.
  // Following this call, the observer CertificatesRefreshed method will be
  // called so the view can call FilterAndBuildOrgGroupingMap as necessary to
  // refresh its tree views.
  void Refresh();

  // Fill |*out_org_grouping_map| with the certificates matching |filter_type|.
  void FilterAndBuildOrgGroupingMap(net::CertType filter_type,
                                    OrgGroupingMap* out_org_grouping_map) const;

  // Import private keys and certificates from PKCS #12 encoded
  // |data|, using the given |password|. If |is_extractable| is false,
  // mark the private key as unextractable from the slot.
  // Returns a net error code on failure or net::OK on success using the
  // `callback`.
  void ImportFromPKCS12(PK11SlotInfo* slot_info,
                        const std::string& data,
                        const std::u16string& password,
                        bool is_extractable,
                        base::OnceCallback<void(int net_result)> callback);

  // Import user certificate from DER encoded |data|.
  // Returns a net error code on failure.
  int ImportUserCert(const std::string& data);

  // Import CA certificates.
  // Tries to import all the certificates given.  The root will be trusted
  // according to |trust_bits|.  Any certificates that could not be imported
  // will be listed in |not_imported|.
  // |trust_bits| should be a bit field of TRUST* values from NSSCertDatabase.
  // Returns false if there is an internal error, otherwise true is returned and
  // |not_imported| should be checked for any certificates that were not
  // imported.
  bool ImportCACerts(const net::ScopedCERTCertificateList& certificates,
                     net::NSSCertDatabase::TrustBits trust_bits,
                     net::NSSCertDatabase::ImportCertFailureList* not_imported);

  // Import server certificate.  The first cert should be the server cert.  Any
  // additional certs should be intermediate/CA certs and will be imported but
  // not given any trust.
  // Any certificates that could not be imported will be listed in
  // |not_imported|.
  // |trust_bits| can be set to explicitly trust or distrust the certificate, or
  // use TRUST_DEFAULT to inherit trust as normal.
  // Returns false if there is an internal error, otherwise true is returned and
  // |not_imported| should be checked for any certificates that were not
  // imported.
  bool ImportServerCert(
      const net::ScopedCERTCertificateList& certificates,
      net::NSSCertDatabase::TrustBits trust_bits,
      net::NSSCertDatabase::ImportCertFailureList* not_imported);

  // Set trust values for certificate.
  // |trust_bits| should be a bit field of TRUST* values from NSSCertDatabase.
  // Returns true on success or false on failure.
  bool SetCertTrust(CERTCertificate* cert,
                    net::CertType type,
                    net::NSSCertDatabase::TrustBits trust_bits);

  // Remove the cert from the cert database.
  void RemoveFromDatabase(net::ScopedCERTCertificate cert,
                          base::OnceCallback<void(bool /*success*/)> callback);

 private:
  // Called when one of the |certs_sources_| has been updated. Will notify the
  // |observer_| that the certificate list has been refreshed.
  void OnCertsSourceUpdated();

  // Finds the |CertsSource| which provided |cert|. Can return nullptr (e.g. if
  // the cert has been deleted in the meantime).
  CertsSource* FindCertsSourceForCert(CERTCertificate* cert);

  // Methods used during initialization, see the comment at the top of the .cc
  // file for details.
  static void DidGetCertDBOnUIThread(
      std::unique_ptr<Params> params,
      CertificateManagerModel::Observer* observer,
      CreationCallback callback,
      net::NSSCertDatabase* cert_db);
  static void DidGetCertDBOnIOThread(
      std::unique_ptr<Params> params,
      CertificateManagerModel::Observer* observer,
      CreationCallback callback,
      net::NSSCertDatabase* cert_db);
  static void GetCertDBOnIOThread(std::unique_ptr<Params> params,
                                  NssCertDatabaseGetter database_getter,
                                  CertificateManagerModel::Observer* observer,
                                  CreationCallback callback);

  raw_ptr<net::NSSCertDatabase> cert_db_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::WeakPtr<kcer::Kcer> kcer_;
#endif

  // CertsSource instances providing certificates. The order matters - if a
  // certificate is provided by more than one CertsSource, only the first one is
  // accepted.
  std::vector<std::unique_ptr<CertsSource>> certs_sources_;

  bool hold_back_updates_ = false;

  // The observer to notify when certificate list is refreshed.
  raw_ptr<Observer> observer_;
};

#endif  // CHROME_BROWSER_CERTIFICATE_MANAGER_MODEL_H_
