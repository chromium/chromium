// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_

#include <set>
#include <string>

#include "base/supports_user_data.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "url/gurl.h"

namespace enterprise_connectors {

// Alias to reduce verbosity when using TriggeredRule::Actions.
using TriggeredRule = ContentAnalysisResponse::Result::TriggeredRule;

// Keys used to read a connector's policy values.
constexpr char kKeyServiceProvider[] = "service_provider";
constexpr char kKeyEnable[] = "enable";
constexpr char kKeyDisable[] = "disable";
constexpr char kKeyUrlList[] = "url_list";
constexpr char kKeyTags[] = "tags";
constexpr char kKeyBlockUntilVerdict[] = "block_until_verdict";
constexpr char kKeyBlockPasswordProtected[] = "block_password_protected";
constexpr char kKeyBlockLargeFiles[] = "block_large_files";
constexpr char kKeyBlockUnsupportedFileTypes[] = "block_unsupported_file_types";
constexpr char kKeyMinimumDataSize[] = "minimum_data_size";

enum class ReportingConnector {
  SECURITY_EVENT,
};

// Enum representing if an analysis should block further interactions with the
// browser until its verdict is obtained.
enum class BlockUntilVerdict {
  NO_BLOCK = 0,
  BLOCK = 1,
};

// Structs representing settings to be used for an analysis or a report. These
// settings should only be kept and considered valid for the specific
// analysis/report they were obtained for.
struct AnalysisSettings {
  AnalysisSettings();
  AnalysisSettings(AnalysisSettings&&);
  AnalysisSettings& operator=(AnalysisSettings&&);
  ~AnalysisSettings();

  GURL analysis_url;
  std::set<std::string> tags;
  BlockUntilVerdict block_until_verdict = BlockUntilVerdict::NO_BLOCK;
  bool block_password_protected_files = false;
  bool block_large_files = false;
  bool block_unsupported_file_types = false;

  // Minimum text size for BulkDataEntry scans. 0 means no minimum.
  size_t minimum_data_size = 100;

  // The DM token to be used for scanning. May be empty, for example if this
  // scan is initiated by APP.
  std::string dm_token = "";
};

struct ReportingSettings {
  ReportingSettings();
  explicit ReportingSettings(GURL url,
                             const std::string& dm_token,
                             bool per_profile);
  ReportingSettings(ReportingSettings&&);
  ReportingSettings& operator=(ReportingSettings&&);
  ~ReportingSettings();

  GURL reporting_url;
  std::string dm_token;

  // Indicates if the report should be made for the profile, or the browser if
  // false.
  bool per_profile = false;
};

// Returns the pref path corresponding to a connector.
const char* ConnectorPref(AnalysisConnector connector);
const char* ConnectorPref(ReportingConnector connector);
const char* ConnectorScopePref(AnalysisConnector connector);
const char* ConnectorScopePref(ReportingConnector connector);

// Returns the highest precedence action in the given parameters.
TriggeredRule::Action GetHighestPrecedenceAction(
    const ContentAnalysisResponse& response);
TriggeredRule::Action GetHighestPrecedenceAction(
    const TriggeredRule::Action& action_1,
    const TriggeredRule::Action& action_2);

// User data class to persist ContentAnalysisResponses in base::SupportsUserData
// objects.
struct ScanResult : public base::SupportsUserData::Data {
  explicit ScanResult(const ContentAnalysisResponse& response);
  ~ScanResult() override;
  static const char kKey[];
  ContentAnalysisResponse response;
};

// Checks if |response| contains a negative malware verdict.
bool ContainsMalwareVerdict(const ContentAnalysisResponse& response);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
