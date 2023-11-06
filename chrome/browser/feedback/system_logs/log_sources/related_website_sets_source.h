// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_RELATED_WEBSITE_SETS_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_RELATED_WEBSITE_SETS_SOURCE_H_

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Get information about the contents of the Related Website Sets in use.
class RelatedWebsiteSetsSource : public system_logs::SystemLogsSource {
 public:
  // The field name.
  static constexpr char kSetsInfoField[] = "Related Website Sets";

  RelatedWebsiteSetsSource();

  RelatedWebsiteSetsSource(const RelatedWebsiteSetsSource&) = delete;
  RelatedWebsiteSetsSource& operator=(const RelatedWebsiteSetsSource&) = delete;

  ~RelatedWebsiteSetsSource() override;

  // SystemLogsSource:
  void Fetch(system_logs::SysLogsSourceCallback callback) override;

 private:
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_RELATED_WEBSITE_SETS_SOURCE_H_
