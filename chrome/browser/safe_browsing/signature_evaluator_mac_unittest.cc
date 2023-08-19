// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/signature_evaluator_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>
#include <string.h>
#include <sys/xattr.h>

#include <string>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/mock_incident_receiver.h"
#include "chrome/common/chrome_paths.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;

namespace safe_browsing {

namespace {

const char* const xattrs[] = {
      "com.apple.cs.CodeDirectory",
      "com.apple.cs.CodeSignature",
      "com.apple.cs.CodeRequirements",
      "com.apple.cs.CodeResources",
      "com.apple.cs.CodeApplication",
      "com.apple.cs.CodeEntitlements",
};

}  // namespace

class MacSignatureEvaluatorTest : public testing::Test {
 protected:
  void SetUp() override {
    base::FilePath source_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &source_path));
    testdata_path_ =
        source_path.AppendASCII("safe_browsing").AppendASCII("mach_o");

    base::FilePath dir_exe;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dir_exe));
    base::FilePath file_exe;
    ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &file_exe));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  bool SetupXattrs(const base::FilePath& path) {
    char sentinel = 'A';
    for (auto* xattr : xattrs) {
      std::vector<uint8_t> buf(10);
      memset(&buf[0], sentinel++, buf.size());
      if (setxattr(path.value().c_str(), xattr, &buf[0], buf.size(), 0, 0) != 0)
        return false;
    }
    return true;
  }

  base::FilePath testdata_path_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(MacSignatureEvaluatorTest, RelativePathComponentTest) {
  EXPECT_FALSE(MacSignatureEvaluator::GetRelativePathComponent(
      base::FilePath("/foo"), base::FilePath("/bar"), nullptr));
  EXPECT_FALSE(MacSignatureEvaluator::GetRelativePathComponent(
      base::FilePath("/foo/bar"), base::FilePath("/bar/baz"), nullptr));
  EXPECT_FALSE(MacSignatureEvaluator::GetRelativePathComponent(
      base::FilePath("/foo/x"), base::FilePath("/foo/y"), nullptr));

  std::string output1;
  EXPECT_TRUE(MacSignatureEvaluator::GetRelativePathComponent(
      base::FilePath("/foo/bar"), base::FilePath("/foo/bar/y"), &output1));
  EXPECT_EQ(output1, "y");

  std::string output2;
  EXPECT_TRUE(MacSignatureEvaluator::GetRelativePathComponent(
      base::FilePath("/Applications/Google Chrome.app"),
      base::FilePath("/Applications/Google Chrome.app/Contents/MacOS/foo"),
      &output2));
  EXPECT_EQ(output2, "Contents/MacOS/foo");
}

TEST_F(MacSignatureEvaluatorTest, SimpleTest) {
  // This is a simple test that checks the validity of a signed executable.
  // There is no designated requirement: we only check the embedded signature.
  base::FilePath path = testdata_path_.AppendASCII("signedexecutablefat");
  MacSignatureEvaluator evaluator(path);
  ASSERT_TRUE(evaluator.Initialize());

  ClientIncidentReport_IncidentData_BinaryIntegrityIncident incident;
  EXPECT_TRUE(evaluator.PerformEvaluation(&incident));
  EXPECT_EQ(0, incident.contained_file_size());
}

TEST_F(MacSignatureEvaluatorTest, SimpleTestWithDR) {
  // This test checks the signer against a designated requirement description.
  base::FilePath path = testdata_path_.AppendASCII("signedexecutablefat");
  std::string requirement(
      "certificate leaf[subject.CN]=\"untrusted@goat.local\"");
  MacSignatureEvaluator evaluator(path, requirement);
  ASSERT_TRUE(evaluator.Initialize());

  ClientIncidentReport_IncidentData_BinaryIntegrityIncident incident;
  EXPECT_TRUE(evaluator.PerformEvaluation(&incident));
  EXPECT_EQ(0, incident.contained_file_size());
}

TEST_F(MacSignatureEvaluatorTest, SimpleTestWithBadDR) {
  // Now test with a designated requirement that does not describe the signer.
  base::FilePath path = testdata_path_.AppendASCII("signedexecutablefat");
  MacSignatureEvaluator evaluator(path, "anchor apple");
  ASSERT_TRUE(evaluator.Initialize());

  ClientIncidentReport_IncidentData_BinaryIntegrityIncident incident;
  EXPECT_FALSE(evaluator.PerformEvaluation(&incident));
  EXPECT_EQ(-67050, incident.sec_error());
  EXPECT_TRUE(incident.has_signature());
  ASSERT_TRUE(incident.has_file_basename());
  EXPECT_EQ("signedexecutablefat", incident.file_basename());
}

TEST_F(MacSignatureEvaluatorTest, SimpleBundleTest) {
  // Now test a simple, validly signed bundle.
  base::FilePath path = testdata_path_.AppendASCII("test-bundle.app");

  std::string requirement(
      "certificate leaf[subject.CN]=\"untrusted@goat.local\"");
  MacSignatureEvaluator evaluator(path, requirement);
  ASSERT_TRUE(evaluator.Initialize());

  ClientIncidentReport_IncidentData_BinaryIntegrityIncident incident;
  EXPECT_TRUE(evaluator.PerformEvaluation(&incident));
  EXPECT_EQ(0, incident.contained_file_size());
}

TEST_F(MacSignatureEvaluatorTest, ModifiedMainExecTest32) {
  // Now to a test modified, signed bundle.
  base::FilePath path = testdata_path_.AppendASCII("modified-main-exec32.app");

  std::string requirement(
      "certificate leaf[subject.CN]=\"untrusted@goat.local\"");
  MacSignatureEvaluator evaluator(path, requirement);
  ASSERT_TRUE(evaluator.Initialize());

  ClientIncidentReport_IncidentData_BinaryIntegrityIncident incident;
  EXPECT_FALSE(evaluator.PerformEvaluation(&incident));
  EXPECT_EQ(-67061, incident.sec_error());
  EXPECT_EQ(path.BaseName().value(), incident.file_basename());
  EXPECT_FALSE(incident.has_signature());
  EXPECT_FALSE(incident.has_image_headers());
  ASSERT_EQ(1, incident.contained_file_size());

  const ClientIncidentReport_IncidentData_BinaryIntegrityIncident_ContainedFile&
      contained_file = incident.contained_file(0);
  EXPECT_EQ(contained_file.relative_path(), "Contents/MacOS/test-bundle");
  EXPECT_TRUE(contained_file.has_signature());
  EXPECT_TRUE(contained_file.has_image_headers());
}

TEST_F(MacSignatureEvaluatorTest, ModifiedMainExecTest64) {
  // Now to a test modified, signed bundle.
  base::FilePath path = testdata_path_.AppendASCII("modified-main-exec64.app");

  std::string requirement(
      "certificate leaf[subject.CN]=\"untrusted@goat.local\"");
  MacSignatureEvaluator evaluator(path, requirement);
  ASSERT_TRUE(evaluator.Initialize());

  ClientIncidentReport_IncidentData_BinaryIntegrityIncident incident;
  EXPECT_FALSE(evaluator.PerformEvaluation(&incident));

  EXPECT_EQ(-67061, incident.sec_error());
  EXPECT_EQ(path.BaseName().value(), incident.file_basename());
  EXPECT_FALSE(incident.has_signature());
  EXPECT_FALSE(incident.has_image_headers());
  ASSERT_EQ(1, incident.contained_file_size());

  const ClientIncidentReport_IncidentData_BinaryIntegrityIncident_ContainedFile&
      contained_file = incident.contained_file(0);
  EXPECT_EQ(contained_file.relative_path(), "Contents/MacOS/test-bundle");
  EXPECT_TRUE(contained_file.has_signature());
  EXPECT_TRUE(contained_file.has_image_headers());
}

TEST_F(MacSignatureEvaluatorTest, ModifiedLocalizationTest) {
  // We want to ignore modifications made to InfoPlist.strings files.
  base::FilePath path = testdata_path_.AppendASCII("modified-localization.app");

  std::string requirement(
      "certificate leaf[subject.CN]=\"untrusted@goat.local\"");
  MacSignatureEvaluator evaluator(path, requirement);
  ASSERT_TRUE(evaluator.Initialize());

  ClientIncidentReport_IncidentData_BinaryIntegrityIncident incident;
  EXPECT_TRUE(evaluator.PerformEvaluation(&incident));
}

TEST_F(MacSignatureEvaluatorTest, ModifiedBundleAndExecTest) {
  // Now test a modified, signed bundle with resources added and the main
  // executable modified.
  base::FilePath path =
      testdata_path_.AppendASCII("modified-bundle-and-exec.app");

  std::string requirement(
      "certificate leaf[subject.CN]=\"untrusted@goat.local\"");
  MacSignatureEvaluator evaluator(path, requirement);
  ASSERT_TRUE(evaluator.Initialize());

  ClientIncidentReport_IncidentData_BinaryIntegrityIncident incident;
  EXPECT_FALSE(evaluator.PerformEvaluation(&incident));
  EXPECT_EQ(-67061, incident.sec_error());
  EXPECT_FALSE(incident.has_signature());
  EXPECT_FALSE(incident.has_image_headers());
  EXPECT_EQ(path.BaseName().value(), incident.file_basename());
  ASSERT_EQ(1, incident.contained_file_size());

  const ClientIncidentReport_IncidentData_BinaryIntegrityIncident_ContainedFile&
      contained_file = incident.contained_file(0);
  EXPECT_EQ(contained_file.relative_path(), "Contents/MacOS/test-bundle");
  EXPECT_TRUE(contained_file.has_signature());
  EXPECT_TRUE(contained_file.has_image_headers());
}

TEST_F(MacSignatureEvaluatorTest, ModifiedBundleTest) {
  // Now test a modified, signed bundle. This bundle has
  // the following problems:
  // 1) A file was added (This should not be reported)
  // 2) libsigned64.dylib was modified
  // 3) executable32 was modified

  base::FilePath orig_path = testdata_path_.AppendASCII("modified-bundle.app");
  base::FilePath copied_path =
      temp_dir_.GetPath().AppendASCII("modified-bundle.app");
  CHECK(base::CopyDirectory(orig_path, copied_path, true));

  // Setup the extended attributes, which don't persist in the git repo.
  ASSERT_TRUE(SetupXattrs(
      copied_path.AppendASCII("Contents/Resources/Base.lproj/MainMenu.nib")));

  std::string requirement(
      "certificate leaf[subject.CN]=\"untrusted@goat.local\"");
  MacSignatureEvaluator evaluator(copied_path, requirement);
  ASSERT_TRUE(evaluator.Initialize());

  ClientIncidentReport_IncidentData_BinaryIntegrityIncident incident;
  EXPECT_FALSE(evaluator.PerformEvaluation(&incident));

  EXPECT_TRUE(incident.has_file_basename());
  EXPECT_EQ(copied_path.BaseName().value(), incident.file_basename());
  EXPECT_FALSE(incident.has_signature());
  EXPECT_FALSE(incident.has_image_headers());
  EXPECT_EQ(-67054, incident.sec_error());
  ASSERT_EQ(4, incident.contained_file_size());

  const ClientIncidentReport_IncidentData_BinaryIntegrityIncident_ContainedFile*
      main_exec = nullptr;
  const ClientIncidentReport_IncidentData_BinaryIntegrityIncident_ContainedFile*
      libsigned64 = nullptr;
  const ClientIncidentReport_IncidentData_BinaryIntegrityIncident_ContainedFile*
      executable32 = nullptr;
  const ClientIncidentReport_IncidentData_BinaryIntegrityIncident_ContainedFile*
      mainmenunib = nullptr;
  const ClientIncidentReport_IncidentData_BinaryIntegrityIncident_ContainedFile*
      codesign_cfg = nullptr;

  for (const auto& contained_file : incident.contained_file()) {
    if (contained_file.relative_path() == "Contents/MacOS/test-bundle") {
      main_exec = &contained_file;
    } else if (contained_file.relative_path() ==
               "Contents/Frameworks/libsigned64.dylib") {
      libsigned64 = &contained_file;
    } else if (contained_file.relative_path() ==
               "Contents/Resources/executable32") {
      executable32 = &contained_file;
    } else if (contained_file.relative_path() ==
               "Contents/Resources/Base.lproj/MainMenu.nib") {
      mainmenunib = &contained_file;
    } else if (contained_file.relative_path() ==
               "Contents/Resources/codesign.cfg") {
      codesign_cfg = &contained_file;
    }
  }

  ASSERT_NE(main_exec, nullptr);
  ASSERT_NE(mainmenunib, nullptr);
  ASSERT_NE(libsigned64, nullptr);
  ASSERT_NE(executable32, nullptr);
  // This is important. Do not collect information on extra files added.
  EXPECT_EQ(codesign_cfg, nullptr);

  EXPECT_TRUE(libsigned64->has_relative_path());
  EXPECT_EQ("Contents/Frameworks/libsigned64.dylib",
            libsigned64->relative_path());
  EXPECT_TRUE(libsigned64->has_signature());

  EXPECT_TRUE(executable32->has_relative_path());
  EXPECT_EQ("Contents/Resources/executable32", executable32->relative_path());
  EXPECT_TRUE(executable32->has_signature());
  EXPECT_TRUE(executable32->has_image_headers());

  EXPECT_TRUE(mainmenunib->has_relative_path());
  EXPECT_EQ("Contents/Resources/Base.lproj/MainMenu.nib",
            mainmenunib->relative_path());
  EXPECT_TRUE(mainmenunib->has_signature());
  EXPECT_EQ(6, mainmenunib->signature().xattr_size());
  // Manually convert the global xattrs array to a vector
  std::vector<std::string> xattrs_known;
  for (auto* xattr : xattrs)
    xattrs_known.push_back(xattr);

  std::vector<std::string> xattrs_seen;
  for (const auto& xattr : mainmenunib->signature().xattr()) {
    ASSERT_TRUE(xattr.has_key());
    EXPECT_TRUE(xattr.has_value());
    xattrs_seen.push_back(xattr.key());
  }
  EXPECT_THAT(xattrs_known, ::testing::ContainerEq(xattrs_seen));
}

}  // namespace safe_browsing
