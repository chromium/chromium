// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_MANAGER_MODEL_H_
#define CHROME_BROWSER_CERTIFICATE_MANAGER_MODEL_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/ssl/client_cert_identity.h"

namespace content {
class BrowserContext;
class ResourceContext;
}  // namespace content

#if defined(OS_CHROMEOS)
namespace chromeos {
class CertificateProvider;
class PolicyCertificateProvider;
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
             base::string16 name,
             bool can_be_deleted,
             bool untrusted,
             Source source,
             bool web_trust_anchor,
             bool hardware_backed,
             bool device_wide);
    ~CertInfo();

    CERTCertificate* cert() const { return cert_.get(); }
    net::CertType type() const { return type_; }
    const base::string16& name() const { return name_; }
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
    base::string16 name_;

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

    DISALLOW_COPY_AND_ASSIGN(CertInfo);

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
#if defined(OS_CHROMEOS)
    // May be nullptr.
    chromeos::PolicyCertificateProvider* policy_certs_provider = nullptr;
    // May be nullptr.
    std::unique_ptr<chromeos::CertificateProvider>
        extension_certificate_provider;
#endif

    Params();
    Params(Params&& other);
    ~Params();

   private:
    DISALLOW_COPY_AND_ASSIGN(Params);
  };

  // Map from the subject organization name to the list of certs from that
  // organization.  If a cert does not have an organization name, the
  // subject's CertPrincipal::GetDisplayName() value is used instead.
  typedef std::map<std::string, std::vector<std::unique_ptr<CertInfo>>>
      OrgGroupingMap;

  typedef base::Callback<void(std::unique_ptr<CertificateManagerModel>)>
      CreationCallback;

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
                     const CreationCallback& callback);

  // Use |Create| instead to create a |CertificateManagerModel| for a
  // |BrowserContext|.
  CertificateManagerModel(std::unique_ptr<Params> params,
                          Observer* observer,
                          net::NSSCertDatabase* nss_cert_database,
                          bool is_user_db_available,
                          bool is_tpm_available);
  ~CertificateManagerModel();

  bool is_user_db_available() const { return is_user_db_available_; }
  bool is_tpm_available() const { return is_tpm_available_; }

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
  // Returns a net error code on failure.
  int ImportFromPKCS12(PK11SlotInfo* slot_info, const std::string& data,
                       const base::string16& password, bool is_extractable);

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

  // Delete the cert.  Returns true on success.  |cert| is still valid when this
  // function returns.
  bool Delete(CERTCertificate* cert);

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
      const CreationCallback& callback,
      net::NSSCertDatabase* cert_db,
      bool is_user_db_available,
      bool is_tpm_available);
  static void DidGetCertDBOnIOThread(
      std::unique_ptr<Params> params,
      CertificateManagerModel::Observer* observer,
      const CreationCallback& callback,
      net::NSSCertDatabase* cert_db);
  static void GetCertDBOnIOThread(std::unique_ptr<Params> params,
                                  content::ResourceContext* resource_context,
                                  CertificateManagerModel::Observer* observer,
                                  const CreationCallback& callback);

  net::NSSCertDatabase* cert_db_;

  // CertsSource instances providing certificates. The order matters - if a
  // certificate is provided by more than one CertsSource, only the first one is
  // accepted.
  std::vector<std::unique_ptr<CertsSource>> certs_sources_;

  bool hold_back_updates_ = false;

  // Whether the certificate database has a public slot associated with the
  // profile. If not set, importing certificates is not allowed with this model.
  bool is_user_db_available_;
  bool is_tpm_available_;

  // The observer to notify when certificate list is refreshed.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(CertificateManagerModel);
};

#endif  // CHROME_BROWSER_CERTIFICATE_MANAGER_MODEL_H_
