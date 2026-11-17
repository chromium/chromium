// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/local_actuation/local_actuation_service_name.h"

#include <string>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

constexpr char kBundleId[] = "com.google.Chrome";
constexpr char kUserDataDir[] =
    "/Users/test/Library/Application Support/Google/Chrome";
// Lowercase hex SHA-256 of `kUserDataDir`.
constexpr char kUserDataDirHash[] =
    "96df6ec27aa0224a20dccf4f893c59bbb729e9697cfed914b90f30573cbccb06";

TEST(LocalActuationServiceNameTest, FormatsBundleIdAndPathHash) {
  EXPECT_EQ(
      GetLocalActuationMachServiceName(kBundleId, base::FilePath(kUserDataDir)),
      base::StrCat({kBundleId, ".local-actuation.", kUserDataDirHash}));
}

TEST(LocalActuationServiceNameTest, IgnoresTrailingSeparators) {
  EXPECT_EQ(GetLocalActuationMachServiceName(
                kBundleId, base::FilePath(base::StrCat({kUserDataDir, "/"}))),
            GetLocalActuationMachServiceName(kBundleId,
                                             base::FilePath(kUserDataDir)));
}

TEST(LocalActuationServiceNameTest, DiffersByUserDataDirAndBundleId) {
  const std::string name =
      GetLocalActuationMachServiceName(kBundleId, base::FilePath(kUserDataDir));

  EXPECT_NE(name, GetLocalActuationMachServiceName(
                      kBundleId, base::FilePath("/tmp/other")));
  EXPECT_NE(name,
            GetLocalActuationMachServiceName("com.google.Chrome.canary",
                                             base::FilePath(kUserDataDir)));
}

TEST(LocalActuationServiceNameTest, DefaultsToBaseBundleId) {
  const base::FilePath user_data_dir(kUserDataDir);

  EXPECT_EQ(GetLocalActuationMachServiceName(user_data_dir),
            GetLocalActuationMachServiceName(base::apple::BaseBundleID(),
                                             user_data_dir));
}

}  // namespace
}  // namespace browser_actuator
