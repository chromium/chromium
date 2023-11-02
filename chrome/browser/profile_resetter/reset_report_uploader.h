// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_H_

#include <list>

#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}

namespace reset_report {
class ChromeResetReport;
}

// Service whose job is up upload ChromeResetReports.
class ResetReportUploader : public KeyedService {
 public:
  explicit ResetReportUploader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ResetReportUploader(const ResetReportUploader&) = delete;
  ResetReportUploader& operator=(const ResetReportUploader&) = delete;

  ~ResetReportUploader() override;

  void DispatchReport(const reset_report::ChromeResetReport& report);

  // Visible for testing:
  void DispatchReportInternal(const std::string& request_data);
  static GURL GetClientReportUrlForTesting();

 private:
  using SimpleURLLoaderList =
      std::list<std::unique_ptr<network::SimpleURLLoader>>;

  void OnSimpleLoaderComplete(SimpleURLLoaderList::iterator it,
                              std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  SimpleURLLoaderList simple_url_loaders_;
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_H_
