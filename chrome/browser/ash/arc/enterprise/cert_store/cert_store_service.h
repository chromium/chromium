// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_CERT_STORE_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_CERT_STORE_SERVICE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/enterprise/cert_store/arc_cert_installer.h"
#include "chrome/services/keymanagement/public/mojom/cert_store_types.mojom.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"

namespace arc {

// This service makes corporate usage keys available to ARC apps.
class CertStoreService : public KeyedService,
                         public net::CertDatabase::Observer {
 public:
  explicit CertStoreService(content::BrowserContext* context);

  // This constructor is public only for testing.
  CertStoreService(content::BrowserContext* context,
                   std::unique_ptr<ArcCertInstaller> installer);

  ~CertStoreService() override;

  CertStoreService(const CertStoreService&) = delete;
  CertStoreService& operator=(const CertStoreService&) = delete;

  // CertDatabase::Observer overrides.
  void OnClientCertStoreChanged() override;

  std::vector<std::string> get_required_cert_names() const {
    return certificate_cache_.get_required_cert_names();
  }

  void set_required_cert_names_for_testing(
      const std::vector<std::string>& cert_names) {
    certificate_cache_.set_required_cert_names_for_testing(
        std::set<std::string>(cert_names.begin(), cert_names.end()));
  }

 private:
  using BuildAllowedCertDescriptionsCallback =
      base::OnceCallback<void(std::vector<CertDescription> allowed_certs)>;
  using FilterAllowedCertificatesCallback =
      base::OnceCallback<void(net::ScopedCERTCertificateList allowed_certs)>;

  //  Stores certificates required in ARC. This is used to check if a policy
  //  update is needed.
  class CertificateCache {
   public:
    CertificateCache();
    CertificateCache(const CertificateCache& other) = delete;
    CertificateCache& operator=(const CertificateCache&) = delete;
    ~CertificateCache();

    // Returns true if new certificates are different from previous ones.
    // If true, policy update is needed.
    bool Update(const std::vector<CertDescription>& certificates);

    std::vector<std::string> get_required_cert_names() const {
      return std::vector<std::string>(required_cert_names_.begin(),
                                      required_cert_names_.end());
    }
    void set_required_cert_names_for_testing(std::set<std::string> cert_names) {
      required_cert_names_ = std::move(cert_names);
    }

   private:
    //  Set of certificates that must be installed in ARC. Corresponds to
    //  corporate usage keys.
    std::set<std::string> required_cert_names_;
  };

  void UpdateCertificates();

  void OnCertificatesListed(keymanagement::mojom::ChapsSlot slot,
                            std::vector<CertDescription> certificates,
                            net::ScopedCERTCertificateList cert_list);

  // Processes |cert_queue| one by one recursively. Certs from from the given
  // |slot|, and are accummulated in the list of |allowed_certs|, which is
  // initially empty. Must be recursive because of async calls.
  void BuildAllowedCertDescriptionsRecursively(
      BuildAllowedCertDescriptionsCallback callback,
      keymanagement::mojom::ChapsSlot slot,
      base::queue<net::ScopedCERTCertificate> cert_queue,
      std::vector<CertDescription> allowed_certs) const;
  // Decides to either proceed to build a |CertDescription| for the given |cert|
  // when it is allowed by |certificate_allowed|, or skip it and proceed to the
  // recursive call to BuildAllowedCertDescriptionsRecursively.
  void BuildAllowedCertDescriptionAndRecurse(
      BuildAllowedCertDescriptionsCallback callback,
      keymanagement::mojom::ChapsSlot slot,
      base::queue<net::ScopedCERTCertificate> cert_queue,
      std::vector<CertDescription> allowed_certs,
      net::ScopedCERTCertificate cert,
      bool certificate_allowed) const;
  // Appends the given |cert_description| to |allowed_certs| and proceeds to the
  // the recursive call to BuildAllowedCertDescriptionsRecursively.
  void AppendCertDescriptionAndRecurse(
      BuildAllowedCertDescriptionsCallback callback,
      keymanagement::mojom::ChapsSlot slot,
      base::queue<net::ScopedCERTCertificate> cert_queue,
      std::vector<CertDescription> allowed_certs,
      std::optional<CertDescription> cert_description) const;
  // Final callback called once all |cert_descriptions| have been processed by
  // BuildAllowedCertDescriptionsRecursively on the given |slot|. May either
  // restart the process to gather certificates on the system slot (when |slot|
  // is the user slot), or proceed to update keymaster keys.
  void OnBuiltAllowedCertDescriptions(
      keymanagement::mojom::ChapsSlot slot,
      std::vector<CertDescription> cert_descriptions) const;
  void OnBuiltAllowedCertDescriptionsForKeymaster(
      keymanagement::mojom::ChapsSlot slot,
      std::vector<CertDescription> cert_descriptions) const;
  void OnBuiltAllowedCertDescriptionsForKeyMint(
      keymanagement::mojom::ChapsSlot slot,
      std::vector<CertDescription> cert_descriptions) const;

  // Done with the user slot, so try to process additional certs in the system
  // slot. If there is no system slot (e.g. the user is not allowed to access
  // it), this call won't mutate |cert_descriptions|, and only return the user
  // slot certificates. However, it's necessary to perform this check
  // asynchronously on the IO thread (through ListCerts), because that's the
  // only thread that knows if the system slot is enabled.
  void ListCertsInSystemSlot(
      std::vector<CertDescription> cert_descriptions) const;

  // Processes metadata from |allowed_certs| stored in the given |slot| and
  // appends them to |certificates|.
  void OnFilteredAllowedCertificates(
      keymanagement::mojom::ChapsSlot slot,
      std::vector<CertDescription> certificates,
      net::ScopedCERTCertificateList allowed_certs);
  void OnUpdatedKeys(std::vector<CertDescription> certificates, bool success);
  void OnArcCertsInstalled(bool need_policy_update, bool success);

  const raw_ptr<content::BrowserContext> context_;

  std::unique_ptr<ArcCertInstaller> installer_;
  CertificateCache certificate_cache_;

  base::WeakPtrFactory<CertStoreService> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_CERT_STORE_SERVICE_H_
