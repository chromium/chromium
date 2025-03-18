// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <utility>

#include "base/android/apk_assets.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/preload/android_apk_assets.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

class PrivacySandboxAttestationsAPKAssetAndroidBrowserTest
    : public AndroidBrowserTest {
 public:
  PrivacySandboxAttestationsAPKAssetAndroidBrowserTest() = default;
  ~PrivacySandboxAttestationsAPKAssetAndroidBrowserTest() override = default;
};

// Check that attestations list exists in Android APK assets. The content of
// the pre-installed attestations list is the same as those delivered via
// component updater. For tests covering the parsing of attestations list, see:
// components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_parser_unittest.cc.
IN_PROC_BROWSER_TEST_F(PrivacySandboxAttestationsAPKAssetAndroidBrowserTest,
                       APKAssetBundledAttestationsList) {
  // base::MemoryMappedFile requires blocking.
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  // Opening the attestations list from APK assets.
  base::MemoryMappedFile::Region region =
      base::MemoryMappedFile::Region::kWholeFile;
  int descriptor = base::android::OpenApkAsset(
      std::string(kAttestationsListAssetPath), &region);
  ASSERT_NE(descriptor, -1);
}

// Check that attestations component manifest exists in Android APK assets.
IN_PROC_BROWSER_TEST_F(PrivacySandboxAttestationsAPKAssetAndroidBrowserTest,
                       APKAssetBundledManifest) {
  // base::MemoryMappedFile requires blocking.
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  // Opening the manifest from APK.
  base::MemoryMappedFile::Region region =
      base::MemoryMappedFile::Region::kWholeFile;
  int descriptor =
      base::android::OpenApkAsset(std::string(kManifestAssetPath), &region);
  ASSERT_NE(descriptor, -1);

  // Create a memory mapped manifest file.
  base::File manifest_file(descriptor);
  base::MemoryMappedFile manifest_memory_mapped_file;
  ASSERT_TRUE(
      manifest_memory_mapped_file.Initialize(std::move(manifest_file), region));

  // Parse the manifest JSON.
  const std::optional<base::Value::Dict> manifest =
      base::JSONReader::ReadDict(base::as_string_view(
          base::as_chars(manifest_memory_mapped_file.bytes())));
  ASSERT_TRUE(manifest.has_value());

  // Manifest should contain a version.
  const std::string* version_lexical = manifest->FindString("version");
  ASSERT_TRUE(version_lexical);
  EXPECT_TRUE(base::IsStringASCII(*version_lexical));

  // Manifest should be labelled as preinstalled.
  const std::optional<bool> is_pre_installed =
      manifest->FindBool("pre_installed");
  ASSERT_EQ(is_pre_installed, true);
}

}  // namespace privacy_sandbox
