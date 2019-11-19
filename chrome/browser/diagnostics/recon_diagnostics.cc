// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/recon_diagnostics.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/diagnostics/diagnostics_test.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/version_info/version_info.h"

#if defined(OS_WIN)
#include "chrome/installer/util/install_util.h"
#endif

// Reconnaissance diagnostics. These are the first and most critical
// diagnostic tests. Here we check for the existence of critical files.
// TODO(cpu): Define if it makes sense to localize strings.

// TODO(cpu): There are a few maximum file sizes hard-coded in this file
// that have little or no theoretical or experimental ground. Find a way
// to justify them.

namespace diagnostics {

namespace {

const int64_t kOneKilobyte = 1024;
const int64_t kOneMegabyte = 1024 * kOneKilobyte;

class InstallTypeTest;
InstallTypeTest* g_install_type = 0;

// Check that the disk space in the volume where the user data directory
// normally lives is not dangerously low.
class DiskSpaceTest : public DiagnosticsTest {
 public:
  DiskSpaceTest() : DiagnosticsTest(DIAGNOSTICS_DISK_SPACE_TEST) {}

  bool ExecuteImpl(DiagnosticsModel::Observer* observer) override {
    base::FilePath data_dir;
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &data_dir))
      return false;
    int64_t disk_space = base::SysInfo::AmountOfFreeDiskSpace(data_dir);
    if (disk_space < 0) {
      RecordFailure(DIAG_RECON_UNABLE_TO_QUERY, "Unable to query free space");
      return true;
    }
    std::string printable_size = base::NumberToString(disk_space);
    if (disk_space < 80 * kOneMegabyte) {
      RecordFailure(DIAG_RECON_LOW_DISK_SPACE,
                    "Low disk space: " + printable_size);
      return true;
    }
    RecordSuccess("Free space: " + printable_size);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DiskSpaceTest);
};

// Check if it is system install or per-user install.
class InstallTypeTest : public DiagnosticsTest {
 public:
  InstallTypeTest()
      : DiagnosticsTest(DIAGNOSTICS_INSTALL_TYPE_TEST), user_level_(false) {}

  bool ExecuteImpl(DiagnosticsModel::Observer* observer) override {
#if defined(OS_WIN)
    user_level_ = InstallUtil::IsPerUserInstall();
    const char* type = user_level_ ? "User Level" : "System Level";
    std::string install_type(type);
#else
    std::string install_type("System Level");
#endif  // defined(OS_WIN)
    RecordSuccess(install_type);
    g_install_type = this;
    return true;
  }

  bool system_level() const { return !user_level_; }

 private:
  bool user_level_;
  DISALLOW_COPY_AND_ASSIGN(InstallTypeTest);
};

// Checks that a given JSON file can be correctly parsed.
class JSONTest : public DiagnosticsTest {
 public:
  enum FileImportance {
    NON_CRITICAL,
    CRITICAL
  };

  JSONTest(const base::FilePath& path,
           DiagnosticsTestId id,
           int64_t max_file_size,
           FileImportance importance)
      : DiagnosticsTest(id),
        path_(path),
        max_file_size_(max_file_size),
        importance_(importance) {}

  bool ExecuteImpl(DiagnosticsModel::Observer* observer) override {
    if (!base::PathExists(path_)) {
      if (importance_ == CRITICAL) {
        RecordOutcome(DIAG_RECON_FILE_NOT_FOUND,
                      "File not found",
                      DiagnosticsModel::TEST_FAIL_CONTINUE);
      } else {
        RecordOutcome(DIAG_RECON_FILE_NOT_FOUND_OK,
                      "File not found (but that is OK)",
                      DiagnosticsModel::TEST_OK);
      }
      return true;
    }
    int64_t file_size;
    if (!base::GetFileSize(path_, &file_size)) {
      RecordFailure(DIAG_RECON_CANNOT_OBTAIN_FILE_SIZE,
                    "Cannot obtain file size");
      return true;
    }

    if (file_size > max_file_size_) {
      RecordFailure(DIAG_RECON_FILE_TOO_BIG, "File too big");
      return true;
    }
    // Being small enough, we can process it in-memory.
    std::string json_data;
    if (!base::ReadFileToString(path_, &json_data)) {
      RecordFailure(DIAG_RECON_UNABLE_TO_OPEN_FILE,
                    "Could not open file. Possibly locked by another process");
      return true;
    }

    JSONStringValueDeserializer json(json_data);
    int error_code = base::JSONReader::JSON_NO_ERROR;
    std::string error_message;
    std::unique_ptr<base::Value> json_root(
        json.Deserialize(&error_code, &error_message));
    if (base::JSONReader::JSON_NO_ERROR != error_code) {
      if (error_message.empty()) {
        error_message = "Parse error " + base::NumberToString(error_code);
      }
      RecordFailure(DIAG_RECON_PARSE_ERROR, error_message);
      return true;
    }

    RecordSuccess("File parsed OK");
    return true;
  }

 private:
  base::FilePath path_;
  int64_t max_file_size_;
  FileImportance importance_;
  DISALLOW_COPY_AND_ASSIGN(JSONTest);
};

// Check that the flavor of the operating system is supported.
class OperatingSystemTest : public DiagnosticsTest {
 public:
  OperatingSystemTest()
      : DiagnosticsTest(DIAGNOSTICS_OPERATING_SYSTEM_TEST) {}

  bool ExecuteImpl(DiagnosticsModel::Observer* observer) override {
    // TODO(port): define the OS criteria for Linux and Mac.
    RecordSuccess(base::StringPrintf(
        "%s %s", base::SysInfo::OperatingSystemName().c_str(),
        base::SysInfo::OperatingSystemVersion().c_str()));
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OperatingSystemTest);
};

struct TestPathInfo {
  DiagnosticsTestId test_id;
  int path_id;
  bool is_directory;
  bool is_optional;
  bool test_writable;
  int64_t max_size;
};

const TestPathInfo kPathsToTest[] = {
    {DIAGNOSTICS_PATH_DICTIONARIES_TEST, chrome::DIR_APP_DICTIONARIES, true,
     true, false, 0},
    {DIAGNOSTICS_PATH_LOCAL_STATE_TEST, chrome::FILE_LOCAL_STATE, false, false,
     true, 500 * kOneKilobyte},
    {DIAGNOSTICS_PATH_RESOURCES_TEST, chrome::FILE_RESOURCES_PACK, false, false,
     false, 0},
    {DIAGNOSTICS_PATH_USER_DATA_TEST, chrome::DIR_USER_DATA, true, false, true,
     850 * kOneMegabyte},
};

// Check that the user's data directory exists and the paths are writable.
class PathTest : public DiagnosticsTest {
 public:
  explicit PathTest(const TestPathInfo& path_info)
      : DiagnosticsTest(path_info.test_id),
        path_info_(path_info) {}

  bool ExecuteImpl(DiagnosticsModel::Observer* observer) override {
    if (!g_install_type) {
      RecordStopFailure(DIAG_RECON_DEPENDENCY, "Install dependency failure");
      return false;
    }
    base::FilePath dir_or_file;
    if (!base::PathService::Get(path_info_.path_id, &dir_or_file)) {
      RecordStopFailure(DIAG_RECON_PATH_PROVIDER, "Path provider failure");
      return false;
    }
    if (!base::PathExists(dir_or_file)) {
      RecordFailure(
          DIAG_RECON_PATH_NOT_FOUND,
          "Path not found: " +
              base::UTF16ToUTF8(dir_or_file.LossyDisplayName()));
      return true;
    }

    int64_t dir_or_file_size = 0;
    if (path_info_.is_directory) {
      dir_or_file_size = base::ComputeDirectorySize(dir_or_file);
    } else {
      base::GetFileSize(dir_or_file, &dir_or_file_size);
    }
    if (!dir_or_file_size && !path_info_.is_optional) {
      RecordFailure(DIAG_RECON_CANNOT_OBTAIN_SIZE,
                    "Cannot obtain size for: " +
                        base::UTF16ToUTF8(dir_or_file.LossyDisplayName()));
      return true;
    }
    std::string printable_size = base::NumberToString(dir_or_file_size);

    if (path_info_.max_size > 0) {
      if (dir_or_file_size > path_info_.max_size) {
        RecordFailure(DIAG_RECON_FILE_TOO_LARGE,
                      "Path contents too large (" + printable_size + ") for: " +
                          base::UTF16ToUTF8(dir_or_file.LossyDisplayName()));
        return true;
      }
    }
    if (!path_info_.test_writable) {
      RecordSuccess("Path exists");
      return true;
    }
    if (!base::PathIsWritable(dir_or_file)) {
      RecordFailure(DIAG_RECON_NOT_WRITABLE,
                    "Path is not writable: " +
                        base::UTF16ToUTF8(dir_or_file.LossyDisplayName()));
      return true;
    }
    RecordSuccess("Path exists and is writable: " + printable_size);
    return true;
  }

 private:
  TestPathInfo path_info_;
  DISALLOW_COPY_AND_ASSIGN(PathTest);
};

// Check the version of Chrome.
class VersionTest : public DiagnosticsTest {
 public:
  VersionTest() : DiagnosticsTest(DIAGNOSTICS_VERSION_TEST) {}

  bool ExecuteImpl(DiagnosticsModel::Observer* observer) override {
    std::string current_version = version_info::GetVersionNumber();
    if (current_version.empty()) {
      RecordFailure(DIAG_RECON_EMPTY_VERSION, "Empty Version");
      return true;
    }
    std::string version_modifier = chrome::GetChannelName();
    if (!version_modifier.empty())
      current_version += " " + version_modifier;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    current_version += " GCB";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    RecordSuccess(current_version);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VersionTest);
};

}  // namespace

std::unique_ptr<DiagnosticsTest> MakeDiskSpaceTest() {
  return std::make_unique<DiskSpaceTest>();
}

std::unique_ptr<DiagnosticsTest> MakeInstallTypeTest() {
  return std::make_unique<InstallTypeTest>();
}

std::unique_ptr<DiagnosticsTest> MakeBookMarksTest() {
  base::FilePath path = DiagnosticsTest::GetUserDefaultProfileDir();
  path = path.Append(bookmarks::kBookmarksFileName);
  return std::make_unique<JSONTest>(path, DIAGNOSTICS_JSON_BOOKMARKS_TEST,
                                    2 * kOneMegabyte, JSONTest::NON_CRITICAL);
}

std::unique_ptr<DiagnosticsTest> MakeLocalStateTest() {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.Append(chrome::kLocalStateFilename);
  return std::make_unique<JSONTest>(path, DIAGNOSTICS_JSON_LOCAL_STATE_TEST,
                                    50 * kOneKilobyte, JSONTest::CRITICAL);
}

std::unique_ptr<DiagnosticsTest> MakePreferencesTest() {
  base::FilePath path = DiagnosticsTest::GetUserDefaultProfileDir();
  path = path.Append(chrome::kPreferencesFilename);
  return std::make_unique<JSONTest>(path, DIAGNOSTICS_JSON_PREFERENCES_TEST,
                                    100 * kOneKilobyte, JSONTest::CRITICAL);
}

std::unique_ptr<DiagnosticsTest> MakeOperatingSystemTest() {
  return std::make_unique<OperatingSystemTest>();
}

std::unique_ptr<DiagnosticsTest> MakeDictonaryDirTest() {
  return std::make_unique<PathTest>(kPathsToTest[0]);
}

std::unique_ptr<DiagnosticsTest> MakeLocalStateFileTest() {
  return std::make_unique<PathTest>(kPathsToTest[1]);
}

std::unique_ptr<DiagnosticsTest> MakeResourcesFileTest() {
  return std::make_unique<PathTest>(kPathsToTest[2]);
}

std::unique_ptr<DiagnosticsTest> MakeUserDirTest() {
  return std::make_unique<PathTest>(kPathsToTest[3]);
}

std::unique_ptr<DiagnosticsTest> MakeVersionTest() {
  return std::make_unique<VersionTest>();
}

}  // namespace diagnostics
