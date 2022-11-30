// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORT_UTIL_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORT_UTIL_H_

#include<string>

#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace history_report {

class UsageReport;

namespace usage_report_util {

std::string ReportToKey(const history_report::UsageReport& report);

bool IsTypedVisit(ui::PageTransition visit_transition);

bool ShouldIgnoreUrl(const GURL& url);

int DatabaseEntries(leveldb::DB* db);

}  // namespace usage_report_util
}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORT_UTIL_H_
