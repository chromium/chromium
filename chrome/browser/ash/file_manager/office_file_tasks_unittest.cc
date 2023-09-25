// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/office_file_tasks.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager::file_tasks {

class FileManagerOfficeFileTasksTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
  }

  TestingProfile* profile() { return profile_.get(); }

  const base::Value::Dict& tasks_by_mime_type() {
    return profile()->GetTestingPrefService()->GetDict(
        prefs::kDefaultTasksByMimeType);
  }

  const base::Value::Dict& tasks_by_suffix() {
    return profile()->GetTestingPrefService()->GetDict(
        prefs::kDefaultTasksBySuffix);
  }

  void ClearPrefs() {
    profile()->GetTestingPrefService()->ClearPref(
        prefs::kDefaultTasksByMimeType);
    profile()->GetTestingPrefService()->ClearPref(prefs::kDefaultTasksBySuffix);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(FileManagerOfficeFileTasksTest,
       UpdateDefaultTask_SetsOfficeFileHandlersForGroup) {
  std::string app_id = "abcdef";
  TaskType task_type = TASK_TYPE_FILE_HANDLER;
  std::string activity = "first_activity";
  TaskDescriptor fake_office_task(app_id, task_type, activity);

  UpdateDefaultTask(profile(), fake_office_task, {".doc"},
                    {"application/msword"});

  std::string expected_task_id = MakeTaskID(app_id, task_type, activity);
  ASSERT_EQ(*tasks_by_mime_type().FindString("application/msword"),
            expected_task_id);
  ASSERT_EQ(*tasks_by_mime_type().FindString(
                "application/"
                "vnd.openxmlformats-officedocument.wordprocessingml.document"),
            expected_task_id);

  ASSERT_EQ(*tasks_by_suffix().FindString(".doc"), expected_task_id);
  ASSERT_EQ(*tasks_by_suffix().FindString(".docx"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".xls"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".pptx"), nullptr);

  ClearPrefs();

  UpdateDefaultTask(profile(), fake_office_task, {".xlsx"},
                    {"application/"
                     "vnd.openxmlformats-officedocument.spreadsheetml.sheet"});

  ASSERT_EQ(*tasks_by_mime_type().FindString("application/vnd.ms-excel"),
            expected_task_id);
  ASSERT_EQ(*tasks_by_mime_type().FindString(
                "application/vnd.ms-excel.sheet.macroEnabled.12"),
            expected_task_id);
  ASSERT_EQ(*tasks_by_mime_type().FindString(
                "application/"
                "vnd.openxmlformats-officedocument.spreadsheetml.sheet"),
            expected_task_id);

  ASSERT_EQ(*tasks_by_suffix().FindString(".xls"), expected_task_id);
  ASSERT_EQ(*tasks_by_suffix().FindString(".xlsm"), expected_task_id);
  ASSERT_EQ(*tasks_by_suffix().FindString(".xlsx"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".doc"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".pptx"), nullptr);

  ClearPrefs();

  UpdateDefaultTask(
      profile(), fake_office_task, {".pptx"},
      {"application/"
       "vnd.openxmlformats-officedocument.presentationml.presentation"});

  ASSERT_EQ(*tasks_by_mime_type().FindString("application/vnd.ms-powerpoint"),
            expected_task_id);
  ASSERT_EQ(
      *tasks_by_mime_type().FindString(
          "application/"
          "vnd.openxmlformats-officedocument.presentationml.presentation"),
      expected_task_id);

  ASSERT_EQ(*tasks_by_suffix().FindString(".ppt"), expected_task_id);
  ASSERT_EQ(*tasks_by_suffix().FindString(".pptx"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".doc"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".xlsm"), nullptr);
}

TEST_F(FileManagerOfficeFileTasksTest,
       UpdateDefaultTask_DoesNotSetOfficeFileHandlersForGroup) {
  std::string app_id = "abcdef";
  TaskType task_type = TASK_TYPE_FILE_HANDLER;
  std::string activity = "first_activity";
  TaskDescriptor fake_office_task(app_id, task_type, activity);

  UpdateDefaultTask(profile(), fake_office_task, {".doc"}, {});

  std::string expected_task_id = MakeTaskID(app_id, task_type, activity);
  ASSERT_EQ(tasks_by_mime_type().FindString("application/msword"), nullptr);
  ASSERT_EQ(tasks_by_mime_type().FindString(
                "application/"
                "vnd.openxmlformats-officedocument.wordprocessingml.document"),
            nullptr);

  ASSERT_EQ(*tasks_by_suffix().FindString(".doc"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".docx"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".xls"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".pptx"), nullptr);

  ClearPrefs();

  UpdateDefaultTask(profile(), fake_office_task, {}, {"application/msword"});

  ASSERT_EQ(*tasks_by_mime_type().FindString("application/msword"),
            expected_task_id);
  ASSERT_EQ(tasks_by_mime_type().FindString(
                "application/"
                "vnd.openxmlformats-officedocument.wordprocessingml.document"),
            nullptr);

  ASSERT_EQ(tasks_by_suffix().FindString(".doc"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".docx"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".xls"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".pptx"), nullptr);

  ClearPrefs();

  UpdateDefaultTask(profile(), fake_office_task, {".doc", ".xls"},
                    {"application/msword", "application/vnd.ms-excel"});

  ASSERT_EQ(*tasks_by_mime_type().FindString("application/msword"),
            expected_task_id);
  ASSERT_EQ(*tasks_by_mime_type().FindString("application/vnd.ms-excel"),
            expected_task_id);
  ASSERT_EQ(tasks_by_mime_type().FindString(
                "application/vnd.ms-excel.sheet.macroEnabled.12"),
            nullptr);

  ASSERT_EQ(*tasks_by_suffix().FindString(".doc"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".docx"), nullptr);
  ASSERT_EQ(*tasks_by_suffix().FindString(".xls"), expected_task_id);
  ASSERT_EQ(tasks_by_suffix().FindString(".xlsm"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".xlsx"), nullptr);
  ASSERT_EQ(tasks_by_suffix().FindString(".pptx"), nullptr);
}

// Test the setting of a default file task for Office files to a Files App SWA.
TEST_F(FileManagerOfficeFileTasksTest, SetOfficeFileHandlersToFilesSWA) {
  TaskDescriptor task(kFileManagerSwaAppId, TaskType::TASK_TYPE_WEB_APP,
                      "chrome://file-manager/?a");

  // Check no default tasks exist for Doc files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), "application/msword", ".doc"));
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(),
      "application/"
      "vnd.openxmlformats-officedocument.wordprocessingml.document",
      ".docx"));
  // Set default task for Doc files as a Files App SWA with action id "a".
  SetWordFileHandlerToFilesSWA(profile(), "a");
  // Check the default task for Doc files is `task`.
  ASSERT_EQ(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                *profile()->GetPrefs(), "application/msword", ".doc"),
            task);
  ASSERT_EQ(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                *profile()->GetPrefs(),
                "application/"
                "vnd.openxmlformats-officedocument.wordprocessingml.document",
                ".docx"),
            task);

  // Check no default tasks exist for Excel files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), "application/vnd.ms-excel", ".xls"));
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(),
      "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
      ".xlsx"));
  // Set default task for Excel files as a Files App SWA with action id "a".
  SetExcelFileHandlerToFilesSWA(profile(), "a");
  // Check the default task for Excel files is `task`.
  ASSERT_EQ(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                *profile()->GetPrefs(), "application/vnd.ms-excel", ".xls"),
            task);
  ASSERT_EQ(
      file_manager::file_tasks::GetDefaultTaskFromPrefs(
          *profile()->GetPrefs(),
          "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
          ".xlsx"),
      task);

  // Check no default tasks exist for Powerpoint files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), "application/vnd.ms-powerpoint", ".ppt"));
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(),
      "application/"
      "vnd.openxmlformats-officedocument.presentationml.presentation",
      ".pptx"));
  // Set default task for Powerpoint files as a Files App SWA with action id
  // "a".
  SetPowerPointFileHandlerToFilesSWA(profile(), "a");
  // Check the default task for Powerpoint files is `task`.
  ASSERT_EQ(
      file_manager::file_tasks::GetDefaultTaskFromPrefs(
          *profile()->GetPrefs(), "application/vnd.ms-powerpoint", ".ppt"),
      task);
  ASSERT_EQ(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                *profile()->GetPrefs(),
                "application/"
                "vnd.openxmlformats-officedocument.presentationml.presentation",
                ".pptx"),
            task);
}

}  // namespace file_manager::file_tasks
