// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_TEST_DLP_FILES_TEST_WITH_MOUNTS_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_TEST_DLP_FILES_TEST_WITH_MOUNTS_H_

#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_event_storage.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_files_test_base.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy {

class MockFilesPolicyNotificationManager;

// Helper class for testing DLP files on Ash and mounting external components.
class DlpFilesTestWithMounts : public DlpFilesTestBase {
 public:
  DlpFilesTestWithMounts(const DlpFilesTestWithMounts&) = delete;
  DlpFilesTestWithMounts& operator=(const DlpFilesTestWithMounts&) = delete;

 protected:
  DlpFilesTestWithMounts();
  ~DlpFilesTestWithMounts() override;

  // Mounts Android, Crostini, Google Drive, and Removable media. This function
  // should be explcitly called in any derived test if needed.
  void MountExternalComponents();

  void SetUp() override;

  void TearDown() override;

  std::unique_ptr<KeyedService> SetFilesPolicyNotificationManager(
      content::BrowserContext* context);

  raw_ptr<MockFilesPolicyNotificationManager, DanglingUntriaged> fpnm_ =
      nullptr;
  std::unique_ptr<DlpFilesControllerAsh> files_controller_;
  std::unique_ptr<reporting::ReportingClient::TestEnvironment>
      reporting_test_enviroment_;
  std::unique_ptr<data_controls::DlpReportingManager> reporting_manager_;
  std::vector<DlpPolicyEvent> events_;
  raw_ptr<DlpFilesEventStorage> event_storage_ = nullptr;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  raw_ptr<storage::ExternalMountPoints> mount_points_ = nullptr;

  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");
  base::FilePath my_files_dir_;
  storage::FileSystemURL my_files_dir_url_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_TEST_DLP_FILES_TEST_WITH_MOUNTS_H_
