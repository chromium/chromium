// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/enterprise/connectors/analysis/analysis_settings.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "content/public/browser/download_manager_delegate.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

namespace enterprise_connectors {

// Alias to reduce verbosity when using TriggeredRule::Actions.
using TriggeredRule = ContentAnalysisResponse::Result::TriggeredRule;

// Pair to specify the source and destination.
using SourceDestinationStringPair = std::pair<std::string, std::string>;

// Keys used to read a connector's policy values.
constexpr char kKeyServiceProvider[] = "service_provider";
constexpr char kKeyLinuxVerification[] = "verification.linux";
constexpr char kKeyMacVerification[] = "verification.mac";
constexpr char kKeyWindowsVerification[] = "verification.windows";
constexpr char kKeyEnable[] = "enable";
constexpr char kKeyDisable[] = "disable";
constexpr char kKeyUrlList[] = "url_list";
constexpr char kKeySourceDestinationList[] = "source_destination_list";
constexpr char kKeyTags[] = "tags";
constexpr char kKeyBlockUntilVerdict[] = "block_until_verdict";
constexpr char kKeyBlockPasswordProtected[] = "block_password_protected";
constexpr char kKeyBlockLargeFiles[] = "block_large_files";
constexpr char kKeyBlockUnsupportedFileTypes[] = "block_unsupported_file_types";
constexpr char kKeyMinimumDataSize[] = "minimum_data_size";
constexpr char kKeyEnabledEventNames[] = "enabled_event_names";
constexpr char kKeyCustomMessages[] = "custom_messages";
constexpr char kKeyRequireJustificationTags[] = "require_justification_tags";
constexpr char kKeyCustomMessagesTag[] = "tag";
constexpr char kKeyCustomMessagesMessage[] = "message";
constexpr char kKeyCustomMessagesLearnMoreUrl[] = "learn_more_url";
constexpr char kKeyMimeTypes[] = "mime_types";
constexpr char kKeyEnterpriseId[] = "enterprise_id";
constexpr char kKeyDomain[] = "domain";
constexpr char kKeyEnabledOptInEvents[] = "enabled_opt_in_events";
constexpr char kKeyOptInEventName[] = "name";
constexpr char kKeyOptInEventUrlPatterns[] = "url_patterns";

// A MIME type string that matches all MIME types.
constexpr char kWildcardMimeType[] = "*";

// The reporting connector subdirectory in User_Data_Directory
constexpr base::FilePath::CharType RC_BASE_DIR[] =
    FILE_PATH_LITERAL("Enterprise/ReportingConnector/");

enum class ReportingConnector {
  SECURITY_EVENT,
};

// Struct holding the necessary data to tweak the behavior of the reporting
// Connector.
struct ReportingSettings {
  ReportingSettings();
  ReportingSettings(GURL url, const std::string& dm_token, bool per_profile);
  ReportingSettings(ReportingSettings&&);
  ReportingSettings(const ReportingSettings&);
  ReportingSettings& operator=(ReportingSettings&&);
  ~ReportingSettings();

  GURL reporting_url;
  std::set<std::string> enabled_event_names;
  std::map<std::string, std::vector<std::string>> enabled_opt_in_events;
  std::string dm_token;

  // Indicates if the report should be made for the profile, or the browser if
  // false.
  bool per_profile = false;
};

// Struct holding the necessary data to tweak the behavior of the file system
// Connector.
struct FileSystemSettings {
  FileSystemSettings();
  FileSystemSettings(const FileSystemSettings&);
  FileSystemSettings(FileSystemSettings&&);
  FileSystemSettings& operator=(const FileSystemSettings&);
  FileSystemSettings& operator=(FileSystemSettings&&);
  ~FileSystemSettings();

  std::string service_provider;
  GURL home;
  GURL authorization_endpoint;
  GURL token_endpoint;
  std::string enterprise_id;
  std::string email_domain;
  std::string client_id;
  std::string client_secret;
  std::vector<std::string> scopes;
  size_t max_direct_size;
  std::set<std::string> mime_types;
};

// Returns the pref path corresponding to a connector.
const char* ConnectorPref(AnalysisConnector connector);
const char* ConnectorPref(ReportingConnector connector);
const char* ConnectorScopePref(AnalysisConnector connector);
const char* ConnectorScopePref(ReportingConnector connector);

// Returns the highest precedence action in the given parameters. Writes the tag
// field of the result containing the highest precedence action into |tag|.
TriggeredRule::Action GetHighestPrecedenceAction(
    const ContentAnalysisResponse& response,
    std::string* tag);
TriggeredRule::Action GetHighestPrecedenceAction(
    const TriggeredRule::Action& action_1,
    const TriggeredRule::Action& action_2);
ContentAnalysisAcknowledgement::FinalAction GetHighestPrecedenceAction(
    const ContentAnalysisAcknowledgement::FinalAction& action_1,
    const ContentAnalysisAcknowledgement::FinalAction& action_2);

// Struct used to persist metadata about a file in base::SupportsUserData
// through ScanResult.
struct FileMetadata {
  FileMetadata(
      const std::string& filename,
      const std::string& sha256,
      const std::string& mime_type,
      int64_t size,
      const ContentAnalysisResponse& scan_response = ContentAnalysisResponse());
  FileMetadata(FileMetadata&&);
  FileMetadata(const FileMetadata&);
  FileMetadata& operator=(const FileMetadata&);
  ~FileMetadata();

  std::string filename;
  std::string sha256;
  std::string mime_type;
  int64_t size;
  ContentAnalysisResponse scan_response;
};

// User data class to persist scanning results for multiple files corresponding
// to a single base::SupportsUserData object.
struct ScanResult : public base::SupportsUserData::Data {
  ScanResult();
  explicit ScanResult(FileMetadata metadata);
  ~ScanResult() override;
  static const char kKey[];

  std::vector<FileMetadata> file_metadata;
  absl::optional<std::u16string> user_justification;
};

// Enum to identify which message to show once scanning is complete. Ordered
// by precedence for when multiple files have conflicting results.
enum class FinalContentAnalysisResult {
  // Show that an issue was found and that the upload is blocked.
  FAILURE = 0,

  // Show that files were not uploaded since they were too large.
  LARGE_FILES = 1,

  // Show that files were not uploaded since they were encrypted.
  ENCRYPTED_FILES = 2,

  // Show that DLP checks failed, but that the user can proceed if they want.
  WARNING = 3,

  // Show that no issue was found and that the user may proceed.
  SUCCESS = 4,
};

// Result for a single request of the RequestHandler classes.
struct RequestHandlerResult {
  bool complies;
  FinalContentAnalysisResult final_result;
  std::string tag;
  std::string request_token;
};

// Calculates the result for the request handler based on the upload result and
// the analysis response.
RequestHandlerResult CalculateRequestHandlerResult(
    const AnalysisSettings& settings,
    safe_browsing::BinaryUploadService::Result upload_result,
    ContentAnalysisResponse response);

// Determines if a request result should be used to allow a data use or to
// block it.
bool ResultShouldAllowDataUse(
    const AnalysisSettings& settings,
    safe_browsing::BinaryUploadService::Result upload_result);

// Calculates the event result that is experienced by the user.
// If data is allowed to be accessed immediately, the result will indicate that
// the user was allowed to use the data independent of the scanning result.
safe_browsing::EventResult CalculateEventResult(
    const AnalysisSettings& settings,
    bool allowed_by_scan_result,
    bool should_warn);

// Calculates the ContentAnalysisAcknowledgement::FinalAction for an action
// based on the response it got from scanning.
ContentAnalysisAcknowledgement::FinalAction GetAckFinalAction(
    const ContentAnalysisResponse& response);

// User data to persist a save package's final callback allowing/denying
// completion. This is used since the callback can be called either when
// scanning completes on a block/allow verdict, when the user cancels the scan,
// or when the user bypasses scanning.
struct SavePackageScanningData : public base::SupportsUserData::Data {
  explicit SavePackageScanningData(
      content::SavePackageAllowedCallback callback);
  ~SavePackageScanningData() override;
  static const char kKey[];

  content::SavePackageAllowedCallback callback;
};

// Checks `item` for a SavePackageScanningData, and run it's callback with
// `allowed` if there is one.
void RunSavePackageScanningCallback(download::DownloadItem* item, bool allowed);

// Checks if |response| contains a negative malware verdict.
bool ContainsMalwareVerdict(const ContentAnalysisResponse& response);

// Returns whether device info should be reported for the profile.
bool IncludeDeviceInfo(Profile* profile, bool per_profile);

// Returns whether the download danger type implies the user should be allowed
// to review the download.
bool ShouldPromptReviewForDownload(Profile* profile,
                                   download::DownloadDangerType danger_type);

// Shows the review dialog after a user has clicked the "Review" button
// corresponding to a download.
void ShowDownloadReviewDialog(const std::u16string& filename,
                              Profile* profile,
                              download::DownloadItem* download_item,
                              content::WebContents* web_contents,
                              download::DownloadDangerType danger_type,
                              base::OnceClosure keep_closure,
                              base::OnceClosure discard_closure);

// Returns true if `result` as returned by FileAnalysisRequest is considered a
// a failed result when attempting a cloud-based content analysis.
bool CloudResultIsFailure(safe_browsing::BinaryUploadService::Result result);

// Returns true if `result` as returned by FileAnalysisRequest is considered a
// a failed result when attempting a local content analysis.
bool LocalResultIsFailure(safe_browsing::BinaryUploadService::Result result);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Returns the single main profile, or nullptr if none is found.
Profile* GetMainProfileLacros();
#endif

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
