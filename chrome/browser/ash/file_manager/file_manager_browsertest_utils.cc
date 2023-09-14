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

TestCase& TestCase::FilesExperimental() {
  options.files_experimental = true;
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

TestCase& TestCase::EnableInlineSyncStatus() {
  options.enable_inline_sync_status = true;
  return *this;
}

TestCase& TestCase::EnableInlineSyncStatusProgressEvents() {
  options.enable_inline_sync_status = true;
  options.enable_inline_sync_status_progress_events = true;
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

TestCase& TestCase::EnableSearchV2() {
  options.enable_search_v2 = true;
  return *this;
}

TestCase& TestCase::EnableFSPsInRecents() {
  options.enable_fsps_in_recents = true;
  return *this;
}

TestCase& TestCase::EnableGoogleOneOfferFilesBanner() {
  options.enable_google_one_offer_files_banner = true;
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
  options.enable_jellybean = true;
  return *this;
}

TestCase& TestCase::EnableImageContentSearch() {
  options.enable_image_content_search = true;
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

  if (options.offline) {
    full_name += "_Offline";
  }

  if (options.files_experimental) {
    full_name += "_FilesExperimental";
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

  if (options.enable_inline_sync_status) {
    full_name += "_InlineSyncStatus";
  }

  if (options.file_transfer_connector_report_only) {
    full_name += "_ReportOnly";
  }

  if (options.enable_search_v2) {
    full_name += "_SearchV2";
  }

  if (options.enable_fsps_in_recents) {
    full_name += "_FSPsInRecents";
  }

  if (options.enable_google_one_offer_files_banner) {
    full_name += "_GoogleOneOfferFilesBanner";
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
