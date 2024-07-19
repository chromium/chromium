// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_REPORTING_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_REPORTING_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/data_controls/content/browser/reporting_service_base.h"
#include "components/enterprise/data_controls/content/browser/reporting_service_base_factory.h"

class Profile;

namespace content {
class BrowserContext;
class ClipboardEndpoint;
struct ClipboardMetadata;
}  // namespace content

namespace data_controls {

class Verdict;

// Keyed service that provides an interface to report Data Control events.
class ReportingService : public ReportingServiceBase {
 public:
  // Converts `source` into a string to be sent in paste reporting events.
  // Depending on what policies are applied and the relationship between
  // `source` and `destination`, the output may be a URL or a special constant
  // (INCOGNITO, CLIPBOARD, OTHER_PROFILE).
  //
  // This function should only be used to obtain a string source for paste
  // reports.
  static std::string GetClipboardSourceString(
      const content::ClipboardEndpoint& source,
      const content::ClipboardEndpoint& destination,
      const char* scope_pref);

  ~ReportingService() override;

  // data_controls::ReportingServiceBase:
  void ReportPaste(const content::ClipboardEndpoint& source,
                   const content::ClipboardEndpoint& destination,
                   const content::ClipboardMetadata& metadata,
                   const Verdict& verdict) override;
  void ReportPasteWarningBypassed(const content::ClipboardEndpoint& source,
                                  const content::ClipboardEndpoint& destination,
                                  const content::ClipboardMetadata& metadata,
                                  const Verdict& verdict) override;
  void ReportCopy(const content::ClipboardEndpoint& source,
                  const content::ClipboardMetadata& metadata,
                  const Verdict& verdict) override;
  void ReportCopyWarningBypassed(const content::ClipboardEndpoint& source,
                                 const content::ClipboardMetadata& metadata,
                                 const Verdict& verdict) override;

 protected:
  friend class ReportingServiceFactory;

  explicit ReportingService(content::BrowserContext& browser_context);

 private:
  void ReportCopyOrPaste(
      const content::ClipboardEndpoint& source,
      const std::optional<content::ClipboardEndpoint>& destination,
      const content::ClipboardMetadata& metadata,
      const Verdict& verdict,
      const std::string& trigger,
      safe_browsing::EventResult event_result);

  // `profile_` is initialized with the browser_context passed in the
  // constructor.
  const raw_ref<Profile> profile_;
};

class ReportingServiceFactory : public ReportingServiceBaseFactory,
                                public ProfileKeyedServiceFactory {
 public:
  // data_controls::ReportingServiceBaseFactory:
  ReportingServiceBase* GetForBrowserContext(
      content::BrowserContext* context) override;

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
