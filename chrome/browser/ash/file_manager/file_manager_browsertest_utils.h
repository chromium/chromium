// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_UTILS_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_UTILS_H_

#include "chrome/browser/ash/file_manager/file_manager_browsertest_base.h"

// INSTANTIATE_TEST_SUITE_P expands to code that stringizes the arguments. Thus
// macro parameters such as |prefix| and |test_class| won't be expanded by the
// macro pre-processor. To work around this, indirect INSTANTIATE_TEST_SUITE_P,
// as WRAPPED_INSTANTIATE_TEST_SUITE_P here, so the pre-processor expands macro
// defines used to disable tests, MAYBE_prefix for example.
#ifndef WRAPPED_INSTANTIATE_TEST_SUITE_P
#define WRAPPED_INSTANTIATE_TEST_SUITE_P(prefix, test_class, generator) \
  INSTANTIATE_TEST_SUITE_P(prefix, test_class, generator,               \
                           &file_manager::test::PostTestCaseName)

#endif  // WRAPPED_INSTANTIATE_TEST_SUITE_P

namespace file_manager {
namespace test {

// Holds the testcase name and FileManagerBrowserTestBase options.
struct TestCase {
  explicit TestCase(const char* const name);

  TestCase& InGuestMode();

  TestCase& InIncognito();

  TestCase& TabletMode();

  TestCase& SetLocale(const std::string& locale);

  TestCase& SetCountry(const std::string& country);

  TestCase& EnableGenericDocumentsProvider();

  TestCase& DisableGenericDocumentsProvider();

  TestCase& EnablePhotosDocumentsProvider();

  TestCase& DisablePhotosDocumentsProvider();

  TestCase& EnableArc();

  TestCase& Offline();

  TestCase& EnableConflictDialog();

  TestCase& DisableNativeSmb();

  TestCase& FakeFileSystemProvider();

  TestCase& DontMountVolumes();

  TestCase& DontObserveFileTasks();

  TestCase& EnableSinglePartitionFormat();

  TestCase& EnableMaterializedViews();

  // Show the startup browser. Some tests invoke the file picker dialog during
  // the test. Requesting a file picker from a background page is forbidden by
  // the apps platform, and it's a bug that these tests do so.
  // FindRuntimeContext() in select_file_dialog_extension.cc will use the last
  // active browser in this case, which requires a Browser to be present. See
  // https://crbug.com/736930.
  TestCase& WithBrowser();

  TestCase& EnableDlp();

  TestCase& EnableFilesPolicyNewUX();

  TestCase& EnableDriveTrash();

  TestCase& EnableUploadOfficeToCloud();

  TestCase& EnableArcVm();

  TestCase& EnableMirrorSync();

  TestCase& EnableFileTransferConnector();

  TestCase& EnableFileTransferConnectorNewUX();

  TestCase& FileTransferConnectorReportOnlyMode();

  TestCase& BypassRequiresJustification();

  TestCase& EnableSearchV2();

  TestCase& EnableLocalImageSearch();

  TestCase& EnableOsFeedback();

  TestCase& DisableGoogleOneOfferFilesBanner();

  TestCase& DisableGoogleOneOfferFilesBannerWithG1Nudge();

  TestCase& FeatureIds(const std::vector<std::string>& ids);

  TestCase& EnableBulkPinning();

  TestCase& EnableDriveShortcuts();

  TestCase& SetDeviceMode(DeviceMode device_mode);

  TestCase& SetTestAccountType(TestAccountType test_account_type);

  TestCase& EnableCrosComponents();

  TestCase& EnableSkyVault();

  std::string GetFullName() const;

  const char* const name;
  FileManagerBrowserTestBase::Options options;
};

std::ostream& operator<<(std::ostream& out, const TestCase& test_case);

// Returns testcase full name.
std::string PostTestCaseName(const ::testing::TestParamInfo<TestCase>& test);

}  // namespace test
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_UTILS_H_
