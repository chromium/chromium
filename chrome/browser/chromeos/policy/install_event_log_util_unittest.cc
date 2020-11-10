// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/install_event_log_util.h"

#include <vector>

#include "base/values.h"
#include "chrome/browser/chromeos/policy/extension_install_event_log.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace em = enterprise_management;

namespace policy {
namespace {

constexpr char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr int64_t kDiskSpaceTotalBytes = 5 * 1024 * 1024;
const int64_t kDiskSpaceFreeBytes = 2 * 1024 * 1024;

const int kExampleFetchTries = 5;
// HTTP_UNAUTHORIZED
const int kExampleResponseCode = 401;

// Common key names used when building the dictionary to pass to the Chrome
// Reporting API. These must be same as ones mentioned in
// install_event_log_util.cc.
constexpr char kEventType[] = "eventType";
constexpr char kOnline[] = "online";
constexpr char kSessionStateChangeType[] = "sessionStateChangeType";
constexpr char kStatefulTotal[] = "statefulTotal";
constexpr char kStatefulFree[] = "statefulFree";
constexpr char kTime[] = "time";

// Key names used for extensions when building the dictionary to pass to the
// Chrome Reporting API. These must be same as ones mentioned in
// install_event_log_util.cc.
constexpr char kExtensionId[] = "extensionId";
constexpr char kExtensionInstallEvent[] = "extensionAppInstallEvent";
constexpr char kDownloadingStage[] = "downloadingStage";
constexpr char kFailureReason[] = "failureReason";
constexpr char kInstallationStage[] = "installationStage";
constexpr char kExtensionType[] = "extensionType";
constexpr char kUserType[] = "userType";
constexpr char kIsNewUser[] = "isNewUser";
constexpr char kIsMisconfigurationFailure[] = "isMisconfigurationFailure";
constexpr char kInstallCreationStage[] = "installCreationStage";
constexpr char kDownloadCacheStatus[] = "downloadCacheStatus";
constexpr char kUnpackerFailureReason[] = "unpackerFailureReason";
constexpr char kManifestInvalidError[] = "manifestInvalidError";
constexpr char kCrxInstallErrorDetail[] = "crxInstallErrorDetail";
constexpr char kFetchErrorCode[] = "fetchErrorCode";
constexpr char kFetchTries[] = "fetchTries";

void ConvertToValueAndVerify(const em::ExtensionInstallReportLogEvent& event,
                             const std::vector<std::string>& keys) {
  base::Value context = reporting::GetContext(nullptr /*profile*/);
  base::Value wrapper;
  wrapper = ConvertExtensionEventToValue(kTestExtensionId, event, context);
  ASSERT_TRUE(wrapper.FindKey(kExtensionInstallEvent) != nullptr);
  EXPECT_TRUE(wrapper.FindKey(kTime) != nullptr);
  base::Value* dict = wrapper.FindKey(kExtensionInstallEvent);
  EXPECT_TRUE(dict->FindKey(kExtensionId) != nullptr);
  for (const std::string& key : keys) {
    EXPECT_TRUE(dict->FindKey(key) != nullptr);
  }
}

}  // namespace

class ExtensionInstallEventLogUtilTest : public testing::Test {
 public:
  ExtensionInstallEventLogUtilTest()
      : scoped_fake_statistics_provider_(
            std::make_unique<
                chromeos::system::ScopedFakeStatisticsProvider>()) {
    event_.set_timestamp(1000);
  }

 protected:
  em::ExtensionInstallReportLogEvent event_;

 private:
  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      scoped_fake_statistics_provider_;
};

// Verifies that an event reporting extension install failure is successfully
// parsed.
TEST_F(ExtensionInstallEventLogUtilTest, FailureReasonEvent) {
  event_.set_event_type(
      em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED);
  event_.set_failure_reason(em::ExtensionInstallReportLogEvent::INVALID_ID);
  event_.set_is_misconfiguration_failure(false);
  event_.set_extension_type(em::Extension_ExtensionType_TYPE_EXTENSION);
  event_.set_stateful_total(kDiskSpaceTotalBytes);
  event_.set_stateful_free(kDiskSpaceFreeBytes);
  ConvertToValueAndVerify(
      event_, {kEventType, kFailureReason, kIsMisconfigurationFailure,
               kExtensionType, kStatefulTotal, kStatefulFree});
}

// Verifies that an event reporting extension installation failure after
// unpacking is successfully parsed.
TEST_F(ExtensionInstallEventLogUtilTest, CrxInstallErrorEvent) {
  event_.set_event_type(
      em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED);
  event_.set_failure_reason(
      em::ExtensionInstallReportLogEvent::CRX_INSTALL_ERROR_OTHER);
  event_.set_crx_install_error_detail(
      em::ExtensionInstallReportLogEvent::UNEXPECTED_ID);
  event_.set_is_misconfiguration_failure(false);
  event_.set_extension_type(em::Extension_ExtensionType_TYPE_EXTENSION);
  event_.set_stateful_total(kDiskSpaceTotalBytes);
  event_.set_stateful_free(kDiskSpaceFreeBytes);
  ConvertToValueAndVerify(
      event_, {kEventType, kFailureReason, kCrxInstallErrorDetail,
               kIsMisconfigurationFailure, kExtensionType, kStatefulTotal,
               kStatefulFree});
}

// Verifies that an event reporting extension unpack failure is successfully
// parsed.
TEST_F(ExtensionInstallEventLogUtilTest, UnpackerFailureReasonEvent) {
  event_.set_event_type(
      em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED);
  event_.set_failure_reason(em::ExtensionInstallReportLogEvent::
                                CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE);
  event_.set_unpacker_failure_reason(
      em::ExtensionInstallReportLogEvent::CRX_HEADER_INVALID);
  event_.set_stateful_total(kDiskSpaceTotalBytes);
  event_.set_stateful_free(kDiskSpaceFreeBytes);
  ConvertToValueAndVerify(event_,
                          {kEventType, kFailureReason, kUnpackerFailureReason,
                           kStatefulTotal, kStatefulFree});
}

// Verifies that an event reporting update manifest invalid error is
// successfully parsed.
TEST_F(ExtensionInstallEventLogUtilTest, ManifestInvalidFailureReasonEvent) {
  event_.set_event_type(
      em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED);
  event_.set_failure_reason(
      em::ExtensionInstallReportLogEvent::MANIFEST_INVALID);
  event_.set_manifest_invalid_error(
      em::ExtensionInstallReportLogEvent::XML_PARSING_FAILED);
  event_.set_stateful_total(kDiskSpaceTotalBytes);
  event_.set_stateful_free(kDiskSpaceFreeBytes);
  ConvertToValueAndVerify(event_,
                          {kEventType, kFailureReason, kManifestInvalidError,
                           kStatefulTotal, kStatefulFree});
}

// Verifies that an event reporting error codes and number of fetch tries when
// extension failed to install with error MANIFEST_FETCH_FAILED is successfully
// parsed.
TEST_F(ExtensionInstallEventLogUtilTest, ManifestFetchFailedEvent) {
  event_.set_event_type(
      em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED);
  event_.set_failure_reason(
      em::ExtensionInstallReportLogEvent::MANIFEST_FETCH_FAILED);
  event_.set_fetch_error_code(kExampleResponseCode);
  event_.set_fetch_tries(kExampleFetchTries);
  event_.set_stateful_total(kDiskSpaceTotalBytes);
  event_.set_stateful_free(kDiskSpaceFreeBytes);
  ConvertToValueAndVerify(event_, {kEventType, kFailureReason, kFetchErrorCode,
                                   kFetchTries, kStatefulTotal, kStatefulFree});
}

// Verifies that an event reporting extension installation stage is successfully
// parsed.
TEST_F(ExtensionInstallEventLogUtilTest, InstallationStageEvent) {
  event_.set_installation_stage(em::ExtensionInstallReportLogEvent::PENDING);
  event_.set_stateful_total(kDiskSpaceTotalBytes);
  event_.set_stateful_free(kDiskSpaceFreeBytes);
  ConvertToValueAndVerify(event_,
                          {kInstallationStage, kStatefulTotal, kStatefulFree});
}

// Verifies that an event reporting extension downloading stage is successfully
// parsed.
TEST_F(ExtensionInstallEventLogUtilTest, DownloadingStageEvent) {
  event_.set_downloading_stage(
      em::ExtensionInstallReportLogEvent::PARSING_MANIFEST);
  event_.set_stateful_total(kDiskSpaceTotalBytes);
  event_.set_stateful_free(kDiskSpaceFreeBytes);
  ConvertToValueAndVerify(event_,
                          {kDownloadingStage, kStatefulTotal, kStatefulFree});
}

// Verifies that a login event reporting user type is successfully parsed.
TEST_F(ExtensionInstallEventLogUtilTest, LoginEvent) {
  event_.set_event_type(
      em::ExtensionInstallReportLogEvent::SESSION_STATE_CHANGE);
  event_.set_session_state_change_type(
      em::ExtensionInstallReportLogEvent::LOGIN);
  event_.set_user_type(em::ExtensionInstallReportLogEvent::USER_TYPE_REGULAR);
  event_.set_is_new_user(false);
  event_.set_online(true);
  ConvertToValueAndVerify(event_, {kEventType, kSessionStateChangeType,
                                   kUserType, kIsNewUser, kOnline});
}

// Verifies that an event reporting extension install creation stage is
// successfully parsed.
TEST_F(ExtensionInstallEventLogUtilTest, InstallCreationStageEvent) {
  event_.set_install_creation_stage(
      em::ExtensionInstallReportLogEvent::CREATION_INITIATED);
  ConvertToValueAndVerify(event_, {kInstallCreationStage});
}

// Verifies that an event reporting cache status during downloading process is
// successfully parsed.
TEST_F(ExtensionInstallEventLogUtilTest, DownloadCacheStatusEvent) {
  event_.set_download_cache_status(
      em::ExtensionInstallReportLogEvent::CACHE_OUTDATED);
  ConvertToValueAndVerify(event_, {kDownloadCacheStatus});
}

}  // namespace policy
