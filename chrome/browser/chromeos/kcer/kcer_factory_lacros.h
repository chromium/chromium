// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KCER_KCER_FACTORY_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_KCER_KCER_FACTORY_LACROS_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/kcer/kcer_factory.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"

namespace content {
class BrowserContext;
}
namespace kcer::internal {
class KcerImpl;
}

namespace kcer {

class KcerFactoryLacros final : public KcerFactory {
 public:
  static void EnsureFactoryBuilt();

 private:
  // Implements KcerFactory.
  bool IsPrimaryContext(content::BrowserContext* context) const override;
  void StartInitializingKcerWithoutNss(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      content::BrowserContext* context) override;
  bool EnsureHighLevelChapsClientInitialized() override;
  void RecordPkcs12CertDualWrittenImpl() override;

  void OnCertDbInfoReceived(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info);
};

}  // namespace kcer

#endif  // CHROME_BROWSER_CHROMEOS_KCER_KCER_FACTORY_LACROS_H_
