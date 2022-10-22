// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace base {
class Clock;
}

namespace content {
class BrowserContext;
}

class CertificateReportingService;

class CertificateReportingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns singleton instance of CertificateReportingServiceFactory.
  static CertificateReportingServiceFactory* GetInstance();

  // Returns the reporting service associated with |context|.
  static CertificateReportingService* GetForBrowserContext(
      content::BrowserContext* context);

  CertificateReportingServiceFactory(
      const CertificateReportingServiceFactory&) = delete;
  CertificateReportingServiceFactory& operator=(
      const CertificateReportingServiceFactory&) = delete;

  // Setters for testing.
  void SetReportEncryptionParamsForTesting(uint8_t* server_public_key,
                                           uint32_t server_public_key_version);
  void SetClockForTesting(base::Clock* clock);
  void SetQueuedReportTTLForTesting(base::TimeDelta queued_report_ttl);
  void SetMaxQueuedReportCountForTesting(size_t max_report_count);
  void SetServiceResetCallbackForTesting(
      const base::RepeatingClosure& service_reset_callback);
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

 private:
  friend struct base::DefaultSingletonTraits<
      CertificateReportingServiceFactory>;

  CertificateReportingServiceFactory();
  ~CertificateReportingServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  // Encryption parameters for certificate reports.
  raw_ptr<uint8_t, DanglingUntriaged> server_public_key_;
  uint32_t server_public_key_version_;

  raw_ptr<base::Clock, DanglingUntriaged> clock_;
  base::TimeDelta queued_report_ttl_;
  size_t max_queued_report_count_;
  base::RepeatingClosure service_reset_callback_;
  scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory_;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_FACTORY_H_
