// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer_mac.h"

#include <Security/Security.h>
#include <stdint.h>

#include <memory>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/mock_incident_receiver.h"
#include "chrome/common/chrome_paths.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;

namespace safe_browsing {

namespace {

const char kBundleBase[] = "test-bundle.app";
const char kBundleURL[] = "test-bundle.app/Contents/MacOS/test-bundle";

// Helper function to corrupt the content of a binary to make sure the signature
// verification will fail.
bool CorruptFileContent(const base::FilePath& file_path) {
  // This is the hard coded position of the TEXT segment in the mach-o file.
  static const uint64_t text_pos = 0x1F90;
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  if (!file.IsValid())
    return false;
  char vec[] = {'\xAA'};
  return UNSAFE_TODO(file.Write(text_pos, vec, sizeof(vec))) == sizeof(vec);
}

}  // namespace

class BinaryIntegrityAnalyzerMacTest : public ::testing::Test {
 public:
  void SetUp() override;

 protected:
  base::FilePath test_data_dir_;
  base::ScopedTempDir temp_dir_;
};

void BinaryIntegrityAnalyzerMacTest::SetUp() {
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
  test_data_dir_ = test_data_dir_.Append("safe_browsing/mach_o/");

  // Set up the temp directory to copy the bundle to.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  // Copy the signed bundle to the temp directory.
  base::FilePath signed_bundle_path =
      base::FilePath(test_data_dir_).Append(kBundleBase);
  base::FilePath copied_bundle_path =
      base::FilePath(temp_dir_.GetPath()).Append(kBundleBase);
  ASSERT_TRUE(
      base::CopyDirectory(signed_bundle_path, copied_bundle_path, true));
}

TEST_F(BinaryIntegrityAnalyzerMacTest, GetCriticalPathsAndRequirements) {
  // Expected paths and requirement strings.
  std::vector<PathAndRequirement> paths_and_requirements_expected;

  std::string expected_req =
      "(identifier \"com.google.Chrome\" or "
      "identifier \"com.google.Chrome.beta\" or "
      "identifier \"com.google.Chrome.dev\" or "
      "identifier \"com.google.Chrome.canary\") and "
      "anchor apple generic and "
      "certificate 1[field.1.2.840.113635.100.6.2.6] and "
      "certificate leaf[field.1.2.840.113635.100.6.1.13] and "
      "certificate leaf[subject.OU] = EQHXZ8M8AV";
  paths_and_requirements_expected.push_back(
      PathAndRequirement(base::apple::OuterBundlePath(), expected_req));
  paths_and_requirements_expected.push_back(
      PathAndRequirement(base::apple::FrameworkBundlePath(), expected_req));

  std::vector<PathAndRequirement> paths_and_requirements =
      GetCriticalPathsAndRequirements();

  ASSERT_EQ(2u, paths_and_requirements.size());
  ASSERT_EQ(paths_and_requirements_expected.size(),
            paths_and_requirements.size());

  for (size_t i = 0; i < paths_and_requirements_expected.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "expected path and requirement " << i);

    EXPECT_EQ(paths_and_requirements[i].path,
              paths_and_requirements_expected[i].path);
    EXPECT_EQ(paths_and_requirements[i].requirement,
              paths_and_requirements_expected[i].requirement);

    base::apple::ScopedCFTypeRef<SecRequirementRef> requirement;
    EXPECT_EQ(
        errSecSuccess,
        SecRequirementCreateWithString(
            base::SysUTF8ToCFStringRef(paths_and_requirements[i].requirement)
                .get(),
            kSecCSDefaultFlags, requirement.InitializeInto()));
  }
}

TEST_F(BinaryIntegrityAnalyzerMacTest, VerifyBinaryIntegrityForTesting) {
  std::unique_ptr<MockIncidentReceiver> mock_receiver(
      new StrictMock<MockIncidentReceiver>());
  base::FilePath bundle = temp_dir_.GetPath().Append(kBundleBase);
  std::string requirement(
      "certificate leaf[subject.CN]=\"untrusted@goat.local\"");

  // Run check on valid bundle.
  std::unique_ptr<Incident> incident_to_clear;
  EXPECT_CALL(*mock_receiver, DoClearIncidentForProcess(_))
      .WillOnce(TakeIncident(&incident_to_clear));
  VerifyBinaryIntegrityForTesting(mock_receiver.get(), bundle, requirement);

  ASSERT_TRUE(incident_to_clear);
  ASSERT_EQ(IncidentType::BINARY_INTEGRITY, incident_to_clear->GetType());
  ASSERT_EQ(incident_to_clear->GetKey(), "test-bundle.app");

  base::FilePath exe_path = temp_dir_.GetPath().Append(kBundleURL);
  ASSERT_TRUE(CorruptFileContent(exe_path));

  std::unique_ptr<Incident> incident;
  EXPECT_CALL(*mock_receiver, DoAddIncidentForProcess(_))
      .WillOnce(TakeIncident(&incident));

  VerifyBinaryIntegrityForTesting(mock_receiver.get(), bundle, requirement);

  // Verify that the incident report contains the expected data.
  std::unique_ptr<ClientIncidentReport_IncidentData> incident_data(
      incident->TakePayload());

  ASSERT_TRUE(incident_data->has_binary_integrity());
  EXPECT_TRUE(incident_data->binary_integrity().has_file_basename());
  EXPECT_EQ("test-bundle.app",
            incident_data->binary_integrity().file_basename());
  EXPECT_TRUE(incident_data->binary_integrity().has_sec_error());
  EXPECT_EQ(-67061, incident_data->binary_integrity().sec_error());
  EXPECT_FALSE(incident_data->binary_integrity().has_signature());
  EXPECT_EQ(0,
            incident_data->binary_integrity().signature().signed_data_size());
  EXPECT_EQ(1, incident_data->binary_integrity().contained_file_size());

  const auto& contained_file =
      incident_data->binary_integrity().contained_file(0);
  EXPECT_EQ(contained_file.relative_path(), "Contents/MacOS/test-bundle");
  EXPECT_TRUE(contained_file.has_signature());
  EXPECT_TRUE(contained_file.has_image_headers());
}

}  // namespace safe_browsing
