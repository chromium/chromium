// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer_win.h"

#include <windows.h>

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/mock_incident_receiver.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;

namespace safe_browsing {

namespace {

const wchar_t kChromeDll[] = L"chrome.dll";
const wchar_t kChromeChildDll[] = L"chrome_child.dll";
const wchar_t kChromeElfDll[] = L"chrome_elf.dll";
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kSignedBinaryDll[] = L"signed_binary.dll";

// Helper function to erase the content of a binary to make sure the signature
// verification will fail.
bool EraseFileContent(const base::FilePath& file_path) {
  FILE* file = base::OpenFile(file_path, "w");

  if (file == NULL)
    return false;

  bool success = base::TruncateFile(file);
  return base::CloseFile(file) && success;
}

}  // namespace

class BinaryIntegrityAnalyzerWinTest : public ::testing::Test {
 public:
  BinaryIntegrityAnalyzerWinTest();

 protected:
  base::FilePath test_data_dir_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> exe_dir_override_;
};

BinaryIntegrityAnalyzerWinTest::BinaryIntegrityAnalyzerWinTest() {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::CreateDirectory(temp_dir_.GetPath().AppendASCII(CHROME_VERSION_STRING));

  // We retrieve DIR_TEST_DATA here because it is based on DIR_EXE and we are
  // about to override the path to the latter.
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_))
    NOTREACHED_IN_MIGRATION();

  exe_dir_override_ = std::make_unique<base::ScopedPathOverride>(
      base::DIR_EXE, temp_dir_.GetPath());
}

TEST_F(BinaryIntegrityAnalyzerWinTest, GetCriticalBinariesPath) {
  // Expected paths.
  std::vector<base::FilePath> critical_binaries_path_expected;
  critical_binaries_path_expected.push_back(
      temp_dir_.GetPath().Append(kChromeExe));
  critical_binaries_path_expected.push_back(
      temp_dir_.GetPath()
          .AppendASCII(CHROME_VERSION_STRING)
          .Append(kChromeDll));
  critical_binaries_path_expected.push_back(
      temp_dir_.GetPath()
          .AppendASCII(CHROME_VERSION_STRING)
          .Append(kChromeChildDll));
  critical_binaries_path_expected.push_back(
      temp_dir_.GetPath()
          .AppendASCII(CHROME_VERSION_STRING)
          .Append(kChromeElfDll));

  std::vector<base::FilePath> critical_binaries_path =
      GetCriticalBinariesPath();

  ASSERT_THAT(critical_binaries_path,
              ::testing::ContainerEq(critical_binaries_path_expected));
}

TEST_F(BinaryIntegrityAnalyzerWinTest, VerifyBinaryIntegrity) {
  // Copy the signed dll to the temp exe directory.
  base::FilePath signed_binary_path(test_data_dir_);
  signed_binary_path =
      signed_binary_path.Append(L"safe_browsing").Append(kSignedBinaryDll);

  base::FilePath chrome_elf_path(temp_dir_.GetPath());
  chrome_elf_path =
      chrome_elf_path.Append(TEXT(CHROME_VERSION_STRING)).Append(kChromeElfDll);

  ASSERT_TRUE(base::CopyFile(signed_binary_path, chrome_elf_path));

  // Run check on an integral binary.
  std::unique_ptr<MockIncidentReceiver> mock_receiver(
      new StrictMock<MockIncidentReceiver>());
  std::unique_ptr<Incident> incident_to_clear;
  EXPECT_CALL(*mock_receiver, DoClearIncidentForProcess(_))
      .WillOnce(TakeIncident(&incident_to_clear));

  VerifyBinaryIntegrity(std::move(mock_receiver));

  ASSERT_TRUE(incident_to_clear);
  ASSERT_EQ(IncidentType::BINARY_INTEGRITY, incident_to_clear->GetType());

  // Run check on an infected binary.
  ASSERT_TRUE(EraseFileContent(chrome_elf_path));

  mock_receiver = std::make_unique<StrictMock<MockIncidentReceiver>>();
  std::unique_ptr<Incident> incident;
  EXPECT_CALL(*mock_receiver, DoAddIncidentForProcess(_))
      .WillOnce(TakeIncident(&incident));

  VerifyBinaryIntegrity(std::move(mock_receiver));

  // Verify that the cleared and reported incidents have the same key.
  ASSERT_EQ(incident->GetKey(), incident_to_clear->GetKey());
  // Verify that the incident report contains the expected data.
  std::unique_ptr<ClientIncidentReport_IncidentData> incident_data(
      incident->TakePayload());
  ASSERT_TRUE(incident_data->has_binary_integrity());
  ASSERT_TRUE(incident_data->binary_integrity().has_file_basename());
  ASSERT_EQ("chrome_elf.dll",
            incident_data->binary_integrity().file_basename());
  ASSERT_TRUE(incident_data->binary_integrity().has_signature());
}

}  // namespace safe_browsing
