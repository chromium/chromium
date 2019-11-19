// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/linux_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPrettyName[] = "PRETTY_NAME";

TEST(LinuxUtilTest, ParseEtcOsReleaseFile) {
  const char kOsRelease[] = R"X(
NAME=Fedora
VERSION="30 (Workstation Edition\)\"
ID=fedora
VERSION_ID=30
VERSION_CODENAME=""
PLATFORM_ID="platform:f30
PRETTY_NAME="Fedora 30 (Workstation Edition)"
ANSI_COLOR="0;34"
LOGO=fedora-logo-icon
CPE_NAME="cpe:/o:fedoraproject:fedora:30"
HOME_URL="https://fedoraproject.org/"
DOCUMENTATION_URL="https://docs.fedoraproject.org/en-US/fedora/f30/system-administrators-guide/"
SUPPORT_URL="https://fedoraproject.org/wiki/Communicating_and_getting_help"
BUG_REPORT_URL="https://bugzilla.redhat.com/"
REDHAT_BUGZILLA_PRODUCT="Fedora"
REDHAT_BUGZILLA_PRODUCT_VERSION=30
REDHAT_SUPPORT_PRODUCT="Fedora"
REDHAT_SUPPORT_PRODUCT_VERSION=30
PRIVACY_POLICY_URL="https://fedoraproject.org/wiki/Legal:PrivacyPolicy"
VARIANT="Workstation Edition"
VARIANT_ID=workstation)X";

  const char kOsReleaseMissingPrettyName[] = R"(
NAME=Fedora
VERSION='30 (Workstation Edition)'
VARIANT_ID=workstation)";

  std::string value =
      base::GetKeyValueFromOSReleaseFileForTesting(kOsRelease, kPrettyName);
  EXPECT_EQ(value, "Fedora 30 (Workstation Edition)");
  // Missing key in the file
  value = base::GetKeyValueFromOSReleaseFileForTesting(
      kOsReleaseMissingPrettyName, kPrettyName);
  EXPECT_EQ(value, "");
  // Value quoted with single ticks
  value = base::GetKeyValueFromOSReleaseFileForTesting(
      kOsReleaseMissingPrettyName, "VERSION");
  EXPECT_EQ(value, "30 (Workstation Edition)");
  // Empty file
  value = base::GetKeyValueFromOSReleaseFileForTesting("", kPrettyName);
  EXPECT_EQ(value, "");
  // Misspelled key
  value =
      base::GetKeyValueFromOSReleaseFileForTesting(kOsRelease, "PRETY_NAME");
  EXPECT_EQ(value, "");
  // Broken key=value format
  value = base::GetKeyValueFromOSReleaseFileForTesting("A/B", kPrettyName);
  EXPECT_EQ(value, "");
  // Empty values
  value =
      base::GetKeyValueFromOSReleaseFileForTesting("PRETTY_NAME=", kPrettyName);
  EXPECT_EQ(value, "");
  value = base::GetKeyValueFromOSReleaseFileForTesting("PRETTY_NAME=\"\"",
                                                       kPrettyName);
  EXPECT_EQ(value, "");
  // Only one key=value in the whole file
  value = base::GetKeyValueFromOSReleaseFileForTesting("PRETTY_NAME=\"Linux\"",
                                                       kPrettyName);
  EXPECT_EQ(value, "Linux");
}

}  // namespace
