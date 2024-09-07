// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_browsertest_utils.h"

namespace file_manager {
namespace test {

TestCase::TestCase(const char* const name) : name(name) {
  CHECK(name && *name) << "no test case name";
}

TestCase& TestCase::InGuestMode() {
  options.guest_mode = IN_GUEST_MODE;
  return *this;
}

TestCase& TestCase::InIncognito() {
  options.guest_mode = IN_INCOGNITO;
  return *this;
}

TestCase& TestCase::TabletMode() {
  options.tablet_mode = true;
  return *this;
}

TestCase& TestCase::SetLocale(const std::string& locale) {
  options.locale = locale;
  return *this;
}

TestCase& TestCase::SetCountry(const std::string& country) {
  options.country = country;
  return *this;
}

TestCase& TestCase::EnableGenericDocumentsProvider() {
  options.arc = true;
  options.generic_documents_provider = true;
  return *this;
}

TestCase& TestCase::DisableGenericDocumentsProvider() {
  options.generic_documents_provider = false;
  return *this;
}

TestCase& TestCase::EnablePhotosDocumentsProvider() {
  options.arc = true;
  options.photos_documents_provider = true;
  return *this;
}

TestCase& TestCase::DisablePhotosDocumentsProvider() {
  options.photos_documents_provider = false;
  return *this;
}

TestCase& TestCase::EnableArc() {
  options.arc = true;
  return *this;
}

TestCase& TestCase::Offline() {
  options.offline = true;
  return *this;
}

TestCase& TestCase::EnableConflictDialog() {
  options.enable_conflict_dialog = true;
  return *this;
}

TestCase& TestCase::DisableNativeSmb() {
  options.native_smb = false;
  return *this;
}

TestCase& TestCase::FakeFileSystemProvider() {
  options.fake_file_system_provider = true;
  return *this;
}

TestCase& TestCase::DontMountVolumes() {
  options.mount_volumes = false;
  return *this;
}

TestCase& TestCase::DontObserveFileTasks() {
  options.observe_file_tasks = false;
  return *this;
}

TestCase& TestCase::EnableSinglePartitionFormat() {
  options.single_partition_format = true;
  return *this;
}

TestCase& TestCase::EnableMaterializedViews() {
  options.enable_materialized_views = true;
  return *this;
}

// Show the startup browser. Some tests invoke the file picker dialog during
// the test. Requesting a file picker from a background page is forbidden by
// the apps platform, and it's a bug that these tests do so.
// FindRuntimeContext() in select_file_dialog_extension.cc will use the last
// active browser in this case, which requires a Browser to be present. See
// https://crbug.com/736930.
TestCase& TestCase::WithBrowser() {
  options.browser = true;
  return *this;
}

TestCase& TestCase::EnableDlp() {
  options.enable_dlp_files_restriction = true;
  return *this;
}

TestCase& TestCase::EnableFilesPolicyNewUX() {
  options.enable_files_policy_new_ux = true;
  return *this;
}

TestCase& TestCase::EnableDriveTrash() {
  options.enable_drive_trash = true;
  return *this;
}

TestCase& TestCase::EnableUploadOfficeToCloud() {
  options.enable_upload_office_to_cloud = true;
  return *this;
}

TestCase& TestCase::EnableArcVm() {
  options.enable_arc_vm = true;
  return *this;
}

TestCase& TestCase::EnableMirrorSync() {
  options.enable_mirrorsync = true;
  return *this;
}

TestCase& TestCase::EnableFileTransferConnector() {
  options.enable_file_transfer_connector = true;
  return *this;
}

TestCase& TestCase::EnableFileTransferConnectorNewUX() {
  options.enable_file_transfer_connector_new_ux = true;
  return *this;
}

TestCase& TestCase::FileTransferConnectorReportOnlyMode() {
  options.file_transfer_connector_report_only = true;
  return *this;
}

TestCase& TestCase::BypassRequiresJustification() {
  options.bypass_requires_justification = true;
  return *this;
}

TestCase& TestCase::EnableLocalImageSearch() {
  options.enable_local_image_search = true;
  return *this;
}

TestCase& TestCase::DisableGoogleOneOfferFilesBanner() {
  options.enable_google_one_offer_files_banner = false;
  return *this;
}

TestCase& TestCase::DisableGoogleOneOfferFilesBannerWithG1Nudge() {
  options.disable_google_one_offer_files_banner = true;
  return *this;
}

TestCase& TestCase::FeatureIds(const std::vector<std::string>& ids) {
  options.feature_ids = ids;
  return *this;
}

TestCase& TestCase::EnableBulkPinning() {
  options.enable_drive_bulk_pinning = true;
  return *this;
}

TestCase& TestCase::EnableDriveShortcuts() {
  options.enable_drive_shortcuts = true;
  return *this;
}

TestCase& TestCase::SetDeviceMode(DeviceMode device_mode) {
  options.device_mode = device_mode;
  return *this;
}

TestCase& TestCase::SetTestAccountType(TestAccountType test_account_type) {
  options.test_account_type = test_account_type;
  return *this;
}

TestCase& TestCase::EnableCrosComponents() {
  options.enable_cros_components = true;
  return *this;
}

TestCase& TestCase::EnableSkyVault() {
  options.enable_skyvault = true;
  return *this;
}

std::string TestCase::GetFullName() const {
  std::string full_name = name;

  if (options.guest_mode == IN_GUEST_MODE) {
    full_name += "_GuestMode";
  }

  if (options.guest_mode == IN_INCOGNITO) {
    full_name += "_Incognito";
  }

  if (options.tablet_mode) {
    full_name += "_TabletMode";
  }

  if (!options.locale.empty()) {
    // You cannot use `-` in a test case name.
    std::string locale_for_name;
    base::ReplaceChars(options.locale, "-", "_", &locale_for_name);
    full_name += "_" + locale_for_name;
  }

  if (!options.country.empty()) {
    full_name += "_" + options.country;
  }

  if (options.offline) {
    full_name += "_Offline";
  }

  if (options.enable_conflict_dialog) {
    full_name += "_ConflictDialog";
  }

  if (!options.native_smb) {
    full_name += "_DisableNativeSmb";
  }

  if (options.generic_documents_provider) {
    full_name += "_GenericDocumentsProvider";
  }

  if (options.photos_documents_provider) {
    full_name += "_PhotosDocumentsProvider";
  }

  if (options.single_partition_format) {
    full_name += "_SinglePartitionFormat";
  }

  if (options.enable_drive_trash) {
    full_name += "_DriveTrash";
  }

  if (options.enable_mirrorsync) {
    full_name += "_MirrorSync";
  }

  if (options.file_transfer_connector_report_only) {
    full_name += "_ReportOnly";
  }

  if (options.enable_local_image_search) {
    full_name += "_LocalImageSearch";
  }

  // Google One offer is enabled by default. Append it to a test name only if
  // it's different from the default value.
  // TODO(b/315829911): Remove Google One offer files banner flag.
  if (!options.enable_google_one_offer_files_banner) {
    full_name += "_DisableGoogleOneOfferFilesBanner";
  }

  // Google One offer is disabled by default. Append it to a test name only if
  // it's different from the default value.
  // TODO(b/315829911): Remove Google One offer files banner flag.
  if (options.disable_google_one_offer_files_banner) {
    full_name += "_DisableGoogleOneOfferFilesBannerWithNudge";
  }

  if (options.enable_drive_bulk_pinning) {
    full_name += "_DriveBulkPinning";
  }

  if (options.enable_drive_shortcuts) {
    full_name += "_DriveShortcuts";
  }

  if (options.enable_cros_components) {
    full_name += "_CrosComponents";
  }

  if (options.enable_materialized_views) {
    full_name += "_MaterializedViews";
  }

  switch (options.device_mode) {
    case kDeviceModeNotSet:
      break;
    case kConsumerOwned:
      full_name += "_DeviceModeConsumerOwned";
      break;
    case kEnrolled:
      full_name += "_DeviceModeEnrolled";
  }

  switch (options.test_account_type) {
    case kTestAccountTypeNotSet:
      break;
    case kEnterprise:
      full_name += "_AccountTypeEnterprise";
      break;
    case kChild:
      full_name += "_AccountTypeChild";
      break;
    case kNonManaged:
      full_name += "_AccountTypeNonManaged";
      break;
    case kNonManagedNonOwner:
      full_name += "_AccountTypeNonManagedNonOwner";
      break;
    case kGoogler:
      full_name += "_AccountTypeGoogler";
      break;
  }

  return full_name;
}

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  return out << test_case.options;
}

std::string PostTestCaseName(const ::testing::TestParamInfo<TestCase>& test) {
  return test.param.GetFullName();
}

}  // namespace test
}  // namespace file_manager
