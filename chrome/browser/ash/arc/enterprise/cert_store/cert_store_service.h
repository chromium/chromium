// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_CERT_STORE_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_CERT_STORE_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/enterprise/cert_store/arc_cert_installer.h"
#include "chrome/services/keymaster/public/mojom/cert_store.mojom.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {

// This service makes corporate usage keys available to ARC apps.
class CertStoreService : public KeyedService,
                         public net::CertDatabase::Observer {
 public:
  struct KeyInfo {
    std::string nickname;
    std::string id;
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static CertStoreService* GetForBrowserContext(
      content::BrowserContext* context);

  // Return the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  explicit CertStoreService(content::BrowserContext* context);

  // This constructor is public only for testing.
  CertStoreService(content::BrowserContext* context,
                   std::unique_ptr<ArcCertInstaller> installer);

  ~CertStoreService() override;

  CertStoreService(const CertStoreService&) = delete;
  CertStoreService& operator=(const CertStoreService&) = delete;

  // CertDatabase::Observer overrides.
  void OnCertDBChanged() override;

  // Returns a real nickname and chaps id for a dummy SPKI |dummy_spki|.
  // Returns nullopt if the key is unknown.
  absl::optional<KeyInfo> GetKeyInfoForDummySpki(const std::string& dummy_spki);

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

  // TODO(b/177051802) Some of certificate cache is obsolete. Clean up.
  class CertificateCache {
   public:
    CertificateCache();
    CertificateCache(const CertificateCache& other) = delete;
    CertificateCache& operator=(const CertificateCache&) = delete;
    ~CertificateCache();

    void Update(const std::vector<CertDescription>& certificates);
    void Update(std::map<std::string, std::string> dummy_spki_by_name);

    absl::optional<KeyInfo> GetKeyInfoForDummySpki(
        const std::string& dummy_spki);

    bool need_policy_update() { return need_policy_update_; }
    void clear_need_policy_update() { need_policy_update_ = false; }
    std::vector<std::string> get_required_cert_names() const {
      return std::vector<std::string>(required_cert_names_.begin(),
                                      required_cert_names_.end());
    }
    void set_required_cert_names_for_testing(std::set<std::string> cert_names) {
      required_cert_names_ = std::move(cert_names);
    }

   private:
    bool need_policy_update_ = false;
    std::set<std::string> required_cert_names_;
    // Map dummy SPKI to real key info.
    std::map<std::string, KeyInfo> key_info_by_dummy_spki_cache_;
    // Map cert name to dummy SPKI.
    std::map<std::string, std::string> dummy_spki_by_name_cache_;
    // Intermediate map name to real SPKI.
    std::map<std::string, KeyInfo> key_info_by_name_cache_;
  };

  void UpdateCertificates();

  void OnCertificatesListed(keymaster::mojom::ChapsSlot slot,
                            std::vector<CertDescription> certificates,
                            net::ScopedCERTCertificateList cert_list);

  // Processes |cert_queue| one by one recursively. Certs from from the given
  // |slot|, and are accummulated in the list of |allowed_certs|, which is
  // initially empty. Must be recursive because of async calls.
  void BuildAllowedCertDescriptionsRecursively(
      BuildAllowedCertDescriptionsCallback callback,
      keymaster::mojom::ChapsSlot slot,
      base::queue<net::ScopedCERTCertificate> cert_queue,
      std::vector<CertDescription> allowed_certs) const;
  // Decides to either proceed to build a |CertDescription| for the given |cert|
  // when it is allowed by |certificate_allowed|, or skip it and proceed to the
  // recursive call to BuildAllowedCertDescriptionsRecursively.
  void BuildAllowedCertDescriptionAndRecurse(
      BuildAllowedCertDescriptionsCallback callback,
      keymaster::mojom::ChapsSlot slot,
      base::queue<net::ScopedCERTCertificate> cert_queue,
      std::vector<CertDescription> allowed_certs,
      net::ScopedCERTCertificate cert,
      bool certificate_allowed) const;
  // Appends the given |cert_description| to |allowed_certs| and proceeds to the
  // the recursive call to BuildAllowedCertDescriptionsRecursively.
  void AppendCertDescriptionAndRecurse(
      BuildAllowedCertDescriptionsCallback callback,
      keymaster::mojom::ChapsSlot slot,
      base::queue<net::ScopedCERTCertificate> cert_queue,
      std::vector<CertDescription> allowed_certs,
      absl::optional<CertDescription> cert_description) const;
  // Final callback called once all |cert_descriptions| have been processed by
  // BuildAllowedCertDescriptionsRecursively on the given |slot|. May either
  // restart the process to gather certificates on the system slot (when |slot|
  // is the user slot), or proceed to update keymaster keys.
  void OnBuiltAllowedCertDescriptions(
      keymaster::mojom::ChapsSlot slot,
      std::vector<CertDescription> cert_descriptions) const;

  // Processes metadata from |allowed_certs| stored in the given |slot| and
  // appends them to |certificates|.
  void OnFilteredAllowedCertificates(
      keymaster::mojom::ChapsSlot slot,
      std::vector<CertDescription> certificates,
      net::ScopedCERTCertificateList allowed_certs);
  void OnUpdatedKeymasterKeys(std::vector<CertDescription> certificates,
                              bool success);
  void OnArcCertsInstalled(bool success);

  content::BrowserContext* const context_;

  std::unique_ptr<ArcCertInstaller> installer_;
  CertificateCache certificate_cache_;

  base::WeakPtrFactory<CertStoreService> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_CERT_STORE_SERVICE_H_
