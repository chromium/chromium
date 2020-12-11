// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace base {
class Clock;
}

namespace content {
class BrowserContext;
}

class CertificateReportingService;

class CertificateReportingServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of CertificateReportingServiceFactory.
  static CertificateReportingServiceFactory* GetInstance();

  // Returns the reporting service associated with |context|.
  static CertificateReportingService* GetForBrowserContext(
      content::BrowserContext* context);

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
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  // Encryption parameters for certificate reports.
  uint8_t* server_public_key_;
  uint32_t server_public_key_version_;

  base::Clock* clock_;
  base::TimeDelta queued_report_ttl_;
  size_t max_queued_report_count_;
  base::RepeatingClosure service_reset_callback_;
  scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(CertificateReportingServiceFactory);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_FACTORY_H_
