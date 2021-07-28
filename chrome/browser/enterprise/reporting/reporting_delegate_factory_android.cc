// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"

namespace enterprise_reporting {

std::unique_ptr<BrowserReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetBrowserReportGeneratorDelegate() {
  // TODO(crbug.com/1228835) Implement BrowserReportGenerator::Delegate for
  // Android
  return nullptr;
}

std::unique_ptr<ProfileReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetProfileReportGeneratorDelegate() {
  // TODO(crbug.com/1228841) Implement ProfileReportGeneratior::Delegate for
  // Android
  return nullptr;
}

std::unique_ptr<ReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetReportGeneratorDelegate() {
  return nullptr;
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryAndroid::GetReportSchedulerDelegate() {
  // TODO(crbug.com/1228844) Implement ReportScheduler::Delegate for Android
  return nullptr;
}

std::unique_ptr<RealTimeReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetRealTimeReportGeneratorDelegate() {
  // TODO(crbug.com/1228845) Implement RealTimeReportGenerator::Delegate for
  // Android
  return nullptr;
}

}  // namespace enterprise_reporting
