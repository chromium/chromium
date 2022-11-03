// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/linux_key_persistence_delegate.h"

#include <string>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace {

base::FilePath::CharType kFileName[] = FILE_PATH_LITERAL("test_file");

constexpr char kErrorHistogramFormat[] =
    "Enterprise.DeviceTrust.Persistence.%s.Error";

// Represents gibberish that gets appended to the file.
constexpr char kGibberish[] = "dfnsdfjdsn";

// Represents an OS key.
constexpr char kValidKeyWrappedBase64[] =
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg3VGyKUYrI0M5VOGIw0dh3D0s26"
    "0xeKGcOKZ76A+LTQuhRANCAAQ8rmn96lycvM/"
    "WTQn4FZnjucsKdj2YrUkcG42LWoC2WorIp8BETdwYr2OhGAVBmSVpg9iyi5gtZ9JGZzMceWOJ";

// String containing invalid base64 characters, like % and the whitespace.
constexpr char kInvalidBase64String[] = "? %";

constexpr char kValidHWKeyFileContent[] =
    "{\"signingKey\":"
    "\"MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg3VGyKUYrI0M5VOGIw0dh3D0s"
    "260xeKGcOKZ76A+LTQuhRANCAAQ8rmn96lycvM/"
    "WTQn4FZnjucsKdj2YrUkcG42LWoC2WorIp8BETdwYr2OhGAVBmSVpg9iyi5gtZ9JGZzMceWOJ"
    "\",\"trustLevel\":1}";
constexpr char kValidOSKeyFileContent[] =
    "{\"signingKey\":"
    "\"MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg3VGyKUYrI0M5VOGIw0dh3D0s"
    "260xeKGcOKZ76A+LTQuhRANCAAQ8rmn96lycvM/"
    "WTQn4FZnjucsKdj2YrUkcG42LWoC2WorIp8BETdwYr2OhGAVBmSVpg9iyi5gtZ9JGZzMceWOJ"
    "\",\"trustLevel\":2}";
constexpr char kInvalidTrustLevelKeyFileContent[] =
    "{\"signingKey\":"
    "\"MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg3VGyKUYrI0M5VOGIw0dh3D0s"
    "260xeKGcOKZ76A+LTQuhRANCAAQ8rmn96lycvM/"
    "WTQn4FZnjucsKdj2YrUkcG42LWoC2WorIp8BETdwYr2OhGAVBmSVpg9iyi5gtZ9JGZzMceWOJ"
    "\",\"trustLevel\":100}";

std::vector<uint8_t> ParseKeyWrapped(base::StringPiece encoded_wrapped) {
  std::string decoded_key;
  if (!base::Base64Decode(encoded_wrapped, &decoded_key)) {
    return std::vector<uint8_t>();
  }

  return std::vector<uint8_t>(decoded_key.begin(), decoded_key.end());
}

void ValidateSigningKey(enterprise_connectors::SigningKeyPair* key_pair,
                        BPKUR::KeyTrustLevel trust_level) {
  ASSERT_TRUE(key_pair);

  EXPECT_EQ(trust_level, key_pair->trust_level());

  if (trust_level == BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED) {
    ASSERT_TRUE(!key_pair->key());
    return;
  }

  EXPECT_EQ(BPKUR::CHROME_BROWSER_OS_KEY, trust_level);
  ASSERT_TRUE(key_pair->key());

  // Extract a pubkey should work.
  std::vector<uint8_t> pubkey = key_pair->key()->GetSubjectPublicKeyInfo();
  ASSERT_GT(pubkey.size(), 0u);

  // Signing should work.
  auto signed_data = key_pair->key()->SignSlowly(
      base::as_bytes(base::make_span("data to sign")));
  ASSERT_TRUE(signed_data.has_value());
  ASSERT_GT(signed_data->size(), 0u);
}

}  // namespace

namespace enterprise_connectors {

class LinuxKeyPersistenceDelegateTest : public testing::Test {
 public:
  LinuxKeyPersistenceDelegateTest() {
    EXPECT_TRUE(scoped_dir_.CreateUniqueTempDir());
    LinuxKeyPersistenceDelegate::SetFilePathForTesting(GetKeyFilePath());
  }

 protected:
  base::FilePath GetKeyFilePath() {
    return scoped_dir_.GetPath().Append(kFileName);
  }

  bool CreateFile(base::StringPiece content) {
    return base::WriteFile(GetKeyFilePath(), content);
  }

  std::string GetFileContents() {
    std::string file_contents;
    base::ReadFileToString(GetKeyFilePath(), &file_contents);
    return file_contents;
  }

  base::ScopedTempDir scoped_dir_;
  LinuxKeyPersistenceDelegate persistence_delegate_;
};

// Tests when the file does not exist and a write operation is attempted.
TEST_F(LinuxKeyPersistenceDelegateTest, StoreKeyPair_FileDoesNotExist) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(persistence_delegate_.StoreKeyPair(BPKUR::CHROME_BROWSER_OS_KEY,
                                                  std::vector<uint8_t>()));
  EXPECT_FALSE(base::PathExists(GetKeyFilePath()));

  // Should expect a failure to open persistence storage metric for the store
  // key pair operation.
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kErrorHistogramFormat, "StoreKeyPair"),
      KeyPersistenceError::kOpenPersistenceStorageFailed, 1);
}

// Tests storing a key with an unspecified trust level.
TEST_F(LinuxKeyPersistenceDelegateTest, StoreKeyPair_UnspecifiedKey) {
  base::HistogramTester histogram_tester;

  CreateFile("");
  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(
      BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()));
  EXPECT_EQ("", GetFileContents());

  // Should expect no failure metrics.
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(kErrorHistogramFormat, "StoreKeyPair"), 0);
}

// Tests when a OS key is stored and file contents are modified before storing
// a new OS key pair.
TEST_F(LinuxKeyPersistenceDelegateTest, StoreKeyPair_ValidOSKeyPair) {
  base::HistogramTester histogram_tester;

  CreateFile("");
  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(
      BPKUR::CHROME_BROWSER_OS_KEY, ParseKeyWrapped(kValidKeyWrappedBase64)));
  EXPECT_EQ(kValidOSKeyFileContent, GetFileContents());

  // Modifying file contents
  base::File file = base::File(GetKeyFilePath(),
                               base::File::FLAG_OPEN | base::File::FLAG_APPEND);
  EXPECT_TRUE(file.WriteAtCurrentPos(kGibberish, strlen(kGibberish)) > 0);
  std::string expected_file_contents(kValidOSKeyFileContent);
  expected_file_contents.append(kGibberish);
  EXPECT_EQ(expected_file_contents, GetFileContents());

  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(
      BPKUR::CHROME_BROWSER_OS_KEY, ParseKeyWrapped(kValidKeyWrappedBase64)));
  EXPECT_EQ(kValidOSKeyFileContent, GetFileContents());

  // Should expect no failure metrics.
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(kErrorHistogramFormat, "StoreKeyPair"), 0);
}

// Tests when a hardware key is stored and file contents are modified before
// storing a new hardware key pair.
TEST_F(LinuxKeyPersistenceDelegateTest, StoreKeyPair_ValidHWKeyPair) {
  base::HistogramTester histogram_tester;

  CreateFile("");
  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(
      BPKUR::CHROME_BROWSER_HW_KEY, ParseKeyWrapped(kValidKeyWrappedBase64)));
  EXPECT_EQ(kValidHWKeyFileContent, GetFileContents());

  // Modifying file contents
  base::File file = base::File(GetKeyFilePath(),
                               base::File::FLAG_OPEN | base::File::FLAG_APPEND);
  EXPECT_TRUE(file.WriteAtCurrentPos(kGibberish, strlen(kGibberish)) > 0);
  std::string expected_file_contents(kValidHWKeyFileContent);
  expected_file_contents.append(kGibberish);
  EXPECT_EQ(expected_file_contents, GetFileContents());

  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(
      BPKUR::CHROME_BROWSER_HW_KEY, ParseKeyWrapped(kValidKeyWrappedBase64)));
  EXPECT_EQ(kValidHWKeyFileContent, GetFileContents());

  // Should expect no failure metrics.
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(kErrorHistogramFormat, "StoreKeyPair"), 0);
}

// Tests trying to load a signing key pair when there is no file.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_NoKeyFile) {
  base::HistogramTester histogram_tester;

  auto key_pair = persistence_delegate_.LoadKeyPair();
  EXPECT_FALSE(key_pair);

  // Should expect a metric for failure in reading from the persistence storage
  // for the load key pair operation.
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kErrorHistogramFormat, "LoadKeyPair"),
      KeyPersistenceError::kReadPersistenceStorageFailed, 1);
}

// Tests loading a valid OS signing key pair from a file.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_ValidOSKeyFile) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(CreateFile(kValidOSKeyFileContent));
  auto key_pair = persistence_delegate_.LoadKeyPair();
  ValidateSigningKey(key_pair.get(), BPKUR::CHROME_BROWSER_OS_KEY);

  // Should expect no failure metrics.
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(kErrorHistogramFormat, "LoadKeyPair"), 0);
}

// Tests that loading a Hardware key pair fails since hardware keys
// are not supported on linux.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_ValidHWKeyFile) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(CreateFile(kValidHWKeyFileContent));
  auto key_pair = persistence_delegate_.LoadKeyPair();
  EXPECT_FALSE(key_pair);

  // Should expect an invalid trust level metric for the load key pair
  // operation.
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kErrorHistogramFormat, "LoadKeyPair"),
      KeyPersistenceError::kInvalidTrustLevel, 1);
}

// Tests loading a key pair from a key file with an invalid trust level.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_InvalidTrustLevel) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(CreateFile(kInvalidTrustLevelKeyFileContent));
  auto key_pair = persistence_delegate_.LoadKeyPair();
  EXPECT_FALSE(key_pair);

  // Should expect an invalid trust level metric for the load key pair
  // operation.
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kErrorHistogramFormat, "LoadKeyPair"),
      KeyPersistenceError::kInvalidTrustLevel, 1);
}

// Tests loading a key pair from a key file when the signing key property is
// missing.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_MissingSigningKey) {
  base::HistogramTester histogram_tester;

  const char file_content[] = "{\"trustLevel\":2}";

  ASSERT_TRUE(CreateFile(file_content));
  auto key_pair = persistence_delegate_.LoadKeyPair();
  EXPECT_FALSE(key_pair);

  // Should expect an invalid signing key metric for the load key pair
  // operation.
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kErrorHistogramFormat, "LoadKeyPair"),
      KeyPersistenceError::kKeyPairMissingSigningKey, 1);
}

// Tests loading a key pair from a key file when the trust level property is
// missing.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_MissingTrustLevel) {
  base::HistogramTester histogram_tester;

  const std::string file_content =
      base::StringPrintf("{\"signingKey\":\"%s\"}", kValidKeyWrappedBase64);

  ASSERT_TRUE(CreateFile(file_content));
  auto key_pair = persistence_delegate_.LoadKeyPair();
  EXPECT_FALSE(key_pair);

  // Should expect a missing trust level metric for the load key pair
  // operation.
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kErrorHistogramFormat, "LoadKeyPair"),
      KeyPersistenceError::kKeyPairMissingTrustLevel, 1);
}

// Tests loading a key pair from a key file when the file content is invalid
// (not a JSON dictionary).
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_InvalidContent) {
  base::HistogramTester histogram_tester;

  const char file_content[] = "just some text";

  ASSERT_TRUE(CreateFile(file_content));
  auto key_pair = persistence_delegate_.LoadKeyPair();
  EXPECT_FALSE(key_pair);

  // Should expect an invalid signing key pair format metric for the load key
  // pair operation.
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kErrorHistogramFormat, "LoadKeyPair"),
      KeyPersistenceError::kInvalidSigningKeyPairFormat, 1);
}

// Tests loading a key pair from a key file when there is a valid key, but the
// key file contains random trailing values.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_TrailingGibberish) {
  base::HistogramTester histogram_tester;

  const std::string file_content = base::StringPrintf(
      "{\"signingKey\":\"%s\",\"trustLevel\":2}someother random content",
      kValidKeyWrappedBase64);

  ASSERT_TRUE(CreateFile(file_content));
  auto key_pair = persistence_delegate_.LoadKeyPair();
  EXPECT_FALSE(key_pair);

  // Should expect an invalid signing key pair format metric for the load key
  // pair operation.
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kErrorHistogramFormat, "LoadKeyPair"),
      KeyPersistenceError::kInvalidSigningKeyPairFormat, 1);
}

// Tests loading a key pair from a key file when the key value is not a valid
// base64 encoded string.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_KeyNotBase64) {
  base::HistogramTester histogram_tester;

  const std::string file_content = base::StringPrintf(
      "{\"signingKey\":\"%s\",\"trustLevel\":2}", kInvalidBase64String);

  ASSERT_TRUE(CreateFile(file_content));
  auto key_pair = persistence_delegate_.LoadKeyPair();
  EXPECT_FALSE(key_pair);

  // Should expect a signing key decoding failure metric for the load key pair
  // operation.
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kErrorHistogramFormat, "LoadKeyPair"),
      KeyPersistenceError::kFailureDecodingSigningKey, 1);
}

// Tests the flow of both storing and loading a key.
TEST_F(LinuxKeyPersistenceDelegateTest, StoreAndLoadKeyPair) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(CreateFile(""));
  auto trust_level = BPKUR::CHROME_BROWSER_OS_KEY;
  auto wrapped = ParseKeyWrapped(kValidKeyWrappedBase64);
  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(trust_level, wrapped));

  auto key_pair = persistence_delegate_.LoadKeyPair();
  ValidateSigningKey(key_pair.get(), trust_level);

  // Should expect no failure metrics.
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(kErrorHistogramFormat, "LoadKeyPair"), 0);
}

// Test creating a key pair returns the correct trust level and a signing key.
TEST_F(LinuxKeyPersistenceDelegateTest, CreateKeyPair) {
  base::HistogramTester histogram_tester;

  auto key_pair = persistence_delegate_.CreateKeyPair();
  ValidateSigningKey(key_pair.get(), key_pair->trust_level());

  // Should expect no failure metrics.
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(kErrorHistogramFormat, "CreateKeyPair"), 0);
}

}  // namespace enterprise_connectors
