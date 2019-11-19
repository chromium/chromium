// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_STORE_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_STORE_BRIDGE_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/arc/mojom/cert_store.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_service.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcCertStoreBridge
    : public KeyedService,
      public ConnectionObserver<arc::mojom::CertStoreInstance>,
      public mojom::CertStoreHost,
      public net::CertDatabase::Observer,
      public policy::PolicyService::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcCertStoreBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcCertStoreBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcCertStoreBridge() override;

  // ConnectionObserver<mojom::CertStoreInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // CertStoreHost overrides.
  void ListCertificates(ListCertificatesCallback callback) override;
  void GetKeyCharacteristics(const std::string& alias,
                             GetKeyCharacteristicsCallback callback) override;
  void Begin(const std::string& alias,
             std::vector<mojom::KeyParamPtr> params,
             BeginCallback callback) override;
  void Update(uint64_t operation_handle,
              const std::vector<uint8_t>& data,
              UpdateCallback callback) override;
  void Finish(uint64_t operation_handle, FinishCallback callback) override;
  void Abort(uint64_t operation_handle, AbortCallback callback) override;

  // CertDatabase::Observer overrides.
  void OnCertDBChanged() override;

  // PolicyService overrides.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

 private:
  void OnGetNSSCertDatabaseForProfile(ListCertificatesCallback callback,
                                      net::NSSCertDatabase* database);
  void OnCertificatesListed(ListCertificatesCallback callback,
                            net::ScopedCERTCertificateList cert_list);
  void UpdateFromKeyPermissionsPolicy();
  void UpdateCertificates();

  content::BrowserContext* const context_;
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  policy::PolicyService* policy_service_ = nullptr;
  // Set to true if at least one ARC app is whitelisted by KeyPermissions
  // policy.
  bool channel_enabled_ = false;

  base::WeakPtrFactory<ArcCertStoreBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcCertStoreBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_STORE_BRIDGE_H_
