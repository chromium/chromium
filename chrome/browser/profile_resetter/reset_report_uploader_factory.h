// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_FACTORY_H_
#define CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

class ResetReportUploader;

class ResetReportUploaderFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns singleton instance of ResetReportUploaderFactory.
  static ResetReportUploaderFactory* GetInstance();

  // Returns the ResetReportUploader associated with |context|.
  static ResetReportUploader* GetForBrowserContext(
      content::BrowserContext* context);

  ResetReportUploaderFactory(const ResetReportUploaderFactory&) = delete;
  ResetReportUploaderFactory& operator=(const ResetReportUploaderFactory&) =
      delete;

 private:
  friend base::NoDestructor<ResetReportUploaderFactory>;

  ResetReportUploaderFactory();
  ~ResetReportUploaderFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_FACTORY_H_
