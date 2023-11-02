// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SERIALIZER_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SERIALIZER_H_

#include "base/values.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"

class PrefService;

namespace ash {
namespace cert_provisioning {

class CertProvisioningSerializer {
 public:
  // Creates/updates saved state of the |worker| in preference storage.
  static void SerializeWorkerToPrefs(PrefService* pref_service,
                                     const CertProvisioningWorkerImpl& worker);
  // Deletes saved state of the |worker| from preference storage.
  static void DeleteWorkerFromPrefs(PrefService* pref_service,
                                    const CertProvisioningWorkerImpl& worker);
  // Deserializes saved worker state |saved_worker| into a just created
  // |worker|. Consider using CertProvisioningWorkerFactory::Deserialize
  // instead of calling it directly.
  static bool DeserializeWorker(const base::Value& saved_worker,
                                CertProvisioningWorkerImpl* worker);

 private:
  static base::Value SerializeWorker(const CertProvisioningWorkerImpl& worker);
};

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SERIALIZER_H_
