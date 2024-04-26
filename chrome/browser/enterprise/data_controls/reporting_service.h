// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_REPORTING_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_REPORTING_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
class ClipboardEndpoint;
struct ClipboardMetadata;
}  // namespace content

namespace data_controls {

class Verdict;

// Keyed service that provides an interface to report Data Control events.
class ReportingService : public KeyedService {
 public:
  ~ReportingService() override;

  void ReportPaste(const content::ClipboardEndpoint& source,
                   const content::ClipboardEndpoint& destination,
                   const content::ClipboardMetadata& metadata,
                   const Verdict& verdict);
  void ReportPasteWarningBypass(const content::ClipboardEndpoint& source,
                                const content::ClipboardEndpoint& destination,
                                const content::ClipboardMetadata& metadata,
                                const Verdict& verdict);

 protected:
  friend class ReportingServiceFactory;

  explicit ReportingService(content::BrowserContext& browser_context);

 private:
  void ReportPaste(const content::ClipboardEndpoint& source,
                   const content::ClipboardEndpoint& destination,
                   const content::ClipboardMetadata& metadata,
                   const Verdict& verdict,
                   const std::string& trigger,
                   safe_browsing::EventResult event_result);

  // Returns true if information from `source` can be included in reported
  // events.
  bool IncludeSourceInformation(
      const content::ClipboardEndpoint& source,
      const content::ClipboardEndpoint& destination) const;

  // `profile_` is initialized with the browser_context passed in the
  // constructor.
  const raw_ref<Profile> profile_;
};

class ReportingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ReportingService* GetForBrowserContext(
      content::BrowserContext* context);

  static ReportingServiceFactory* GetInstance();

  ReportingServiceFactory(const ReportingServiceFactory&) = delete;
  ReportingServiceFactory& operator=(const ReportingServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ReportingServiceFactory>;

  ReportingServiceFactory();
  ~ReportingServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_REPORTING_SERVICE_H_
