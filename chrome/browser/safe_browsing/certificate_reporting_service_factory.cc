// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Maximum age in seconds until a report is dropped from the retry list.
// By default, reports older than a day are ignored and never retried again.
static const uint64_t kMaxReportAgeInSeconds = 86400;

// Maximum number of reports to be kept in the report retry list. If an incoming
// report has a more recent creation date than the oldest report in the list,
// the oldest report is removed from the list and the incoming report is added.
// Otherwise, the incoming report is ignored.
const size_t kMaxReportCountInQueue = 5;

}  // namespace

// static
CertificateReportingServiceFactory*
CertificateReportingServiceFactory::GetInstance() {
  static base::NoDestructor<CertificateReportingServiceFactory> instance;
  return instance.get();
}

// static
CertificateReportingService*
CertificateReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CertificateReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

void CertificateReportingServiceFactory::SetReportEncryptionParamsForTesting(
    uint8_t* server_public_key,
    uint32_t server_public_key_version) {
  server_public_key_ = server_public_key;
  server_public_key_version_ = server_public_key_version;
}

void CertificateReportingServiceFactory::SetClockForTesting(
    base::Clock* clock) {
  clock_ = std::move(clock);
}

void CertificateReportingServiceFactory::SetQueuedReportTTLForTesting(
    base::TimeDelta queued_report_ttl) {
  queued_report_ttl_ = queued_report_ttl;
}

void CertificateReportingServiceFactory::SetMaxQueuedReportCountForTesting(
    size_t max_queued_report_count) {
  max_queued_report_count_ = max_queued_report_count;
}

void CertificateReportingServiceFactory::SetServiceResetCallbackForTesting(
    const base::RepeatingClosure& service_reset_callback) {
  service_reset_callback_ = service_reset_callback;
}

void CertificateReportingServiceFactory::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  test_url_loader_factory_ = factory;
}

CertificateReportingServiceFactory::CertificateReportingServiceFactory()
    : ProfileKeyedServiceFactory(
          "cert_reporting::Factory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()),
      server_public_key_(nullptr),
      server_public_key_version_(0),
      clock_(base::DefaultClock::GetInstance()),
      queued_report_ttl_(base::Seconds(kMaxReportAgeInSeconds)),
      max_queued_report_count_(kMaxReportCountInQueue),
      service_reset_callback_(base::DoNothing()) {}

CertificateReportingServiceFactory::~CertificateReportingServiceFactory() =
    default;

KeyedService* CertificateReportingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();
  // In unit tests the safe browsing service can be null, if this happens,
  // return null instead of crashing.
  if (!safe_browsing_service)
    return nullptr;
  Profile* profile = Profile::FromBrowserContext(browser_context);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (test_url_loader_factory_.get()) {
    url_loader_factory = test_url_loader_factory_;
  } else {
    url_loader_factory = profile->GetURLLoaderFactory();
  }
  return new CertificateReportingService(
      safe_browsing_service, url_loader_factory, static_cast<Profile*>(profile),
      server_public_key_, server_public_key_version_, max_queued_report_count_,
      queued_report_ttl_, clock_, service_reset_callback_);
}
