// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SERIALIZER_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SERIALIZER_H_

#include "base/values.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker_static.h"

class PrefService;

namespace ash {
namespace cert_provisioning {

class CertProvisioningSerializer {
 public:
  // Creates/updates saved state of the |worker| in preference storage.
  static void SerializeWorkerToPrefs(
      PrefService* pref_service,
      const CertProvisioningWorkerStatic& worker);
  // Deletes saved state of the |worker| from preference storage.
  static void DeleteWorkerFromPrefs(PrefService* pref_service,
                                    const CertProvisioningWorkerStatic& worker);
  // Deserializes saved worker state |saved_worker| into a just created
  // |worker|. Consider using CertProvisioningWorkerFactory::Deserialize
  // instead of calling it directly.
  static bool DeserializeWorker(const base::Value::Dict& saved_worker,
                                CertProvisioningWorkerStatic* worker);

 private:
  static base::Value::Dict SerializeWorker(
      const CertProvisioningWorkerStatic& worker);
};

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SERIALIZER_H_
