// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CHROME_BROWSER_MAIN_EXTRA_PARTS_ENTERPRISE_H_
#define CHROME_BROWSER_ENTERPRISE_CHROME_BROWSER_MAIN_EXTRA_PARTS_ENTERPRISE_H_

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "components/enterprise/buildflags/buildflags.h"

namespace enterprise_util {

// This class is used to run enterprise code at specific points in the
// chrome browser process life cycle.
class ChromeBrowserMainExtraPartsEnterprise
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsEnterprise();
  ChromeBrowserMainExtraPartsEnterprise(
      const ChromeBrowserMainExtraPartsEnterprise&) = delete;
  ChromeBrowserMainExtraPartsEnterprise& operator=(
      const ChromeBrowserMainExtraPartsEnterprise&) = delete;
  ~ChromeBrowserMainExtraPartsEnterprise() override;

  // ChromeBrowserMainExtraParts:
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)) && \
    BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
#endif
  void PostCreateMainMessageLoop() override;

 private:
  // ERP client instance, serving all reporting needs in the browser.
  reporting::ReportQueueProvider::SmartPtr<reporting::ReportingClient>
      reporting_client_{nullptr, base::OnTaskRunnerDeleter(nullptr)};
};

}  // namespace enterprise_util

#endif  // CHROME_BROWSER_ENTERPRISE_CHROME_BROWSER_MAIN_EXTRA_PARTS_ENTERPRISE_H_
