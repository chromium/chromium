// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_RELATED_WEBSITE_SETS_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_RELATED_WEBSITE_SETS_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace first_party_sets {
class FirstPartySetsPolicyService;
}  // namespace first_party_sets

namespace system_logs {

// Get information about the contents of the Related Website Sets in use.
class RelatedWebsiteSetsSource : public system_logs::SystemLogsSource {
 public:
  // The field name.
  static constexpr char kSetsInfoField[] = "Related Website Sets";

  // `service` must not be nullptr.
  explicit RelatedWebsiteSetsSource(
      first_party_sets::FirstPartySetsPolicyService* service);

  RelatedWebsiteSetsSource(const RelatedWebsiteSetsSource&) = delete;
  RelatedWebsiteSetsSource& operator=(const RelatedWebsiteSetsSource&) = delete;

  ~RelatedWebsiteSetsSource() override;

  // SystemLogsSource:
  void Fetch(system_logs::SysLogsSourceCallback callback) override;

 private:
  base::WeakPtr<first_party_sets::FirstPartySetsPolicyService> service_;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_RELATED_WEBSITE_SETS_SOURCE_H_
