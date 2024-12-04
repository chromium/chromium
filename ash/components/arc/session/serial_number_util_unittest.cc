// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/serial_number_util.h"

#include "ash/components/arc/arc_prefs.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class SerialNumberUtilTest : public testing::Test {
 public:
  SerialNumberUtilTest() {
    arc::prefs::RegisterLocalStatePrefs(test_local_state_.registry());
  }

  SerialNumberUtilTest(const SerialNumberUtilTest&) = delete;
  SerialNumberUtilTest& operator=(const SerialNumberUtilTest&) = delete;

  ~SerialNumberUtilTest() override = default;

 protected:
  PrefService* test_local_state() { return &test_local_state_; }

 private:
  TestingPrefServiceSimple test_local_state_;
};

TEST_F(SerialNumberUtilTest, GenerateFakeSerialNumber) {
  // Check that the function always returns 20-character string.
  EXPECT_EQ(20U,
            GenerateFakeSerialNumber("mytestaccount@gmail.com", "001122aabbcc")
                .size());
  EXPECT_EQ(20U, GenerateFakeSerialNumber("", "").size());
  EXPECT_EQ(20U, GenerateFakeSerialNumber("a", "b").size());

  // Check that the function always returns the same ID for the same
  // account and hwid_raw.
  const std::string id_1 =
      GenerateFakeSerialNumber("mytestaccount@gmail.com", "001122aabbcc");
  const std::string id_2 =
      GenerateFakeSerialNumber("mytestaccount@gmail.com", "001122aabbcc");
  EXPECT_EQ(id_1, id_2);

  // Generate an ID for a different account but for the same machine.
  // Check that the ID is not the same as |id_1|.
  const std::string id_3 =
      GenerateFakeSerialNumber("mytestaccount2@gmail.com", "001122aabbcc");
  EXPECT_NE(id_1, id_3);

  // Generate an ID for a different machine but for the same account.
  // Check that the ID is not the same as |id_1|.
  const std::string id_4 =
      GenerateFakeSerialNumber("mytestaccount@gmail.com", "001122aaddcc");
  EXPECT_NE(id_1, id_4);

  // Check that the function treats '\0' in |salt| properly.
  using std::literals::string_literals::operator""s;
  const std::string id_5 =
      GenerateFakeSerialNumber("mytestaccount@gmail.com", "a\0b"s);
  const std::string id_6 =
      GenerateFakeSerialNumber("mytestaccount@gmail.com", "a\0c"s);
  EXPECT_NE(id_5, id_6);
}

TEST_F(SerialNumberUtilTest, GetOrCreateSerialNumber) {
  constexpr size_t kSerialNumberLen = 20;
  constexpr size_t kHexSaltLen = 32;

  const std::string chromeos_user = "user@gmail.com";
  const std::string chromeos_user2 = "user2@gmail.com";
  ASSERT_TRUE(
      test_local_state()->GetString(prefs::kArcSerialNumberSalt).empty());

  // Check that when neither the pref nor the salt file exists, a random salt
  // is stored in the local state, and a serial number based on the salt is
  // returned.
  const std::string serialno_1 =
      GetOrCreateSerialNumber(test_local_state(), chromeos_user, std::string());
  EXPECT_FALSE(serialno_1.empty());
  EXPECT_EQ(kSerialNumberLen, serialno_1.size());

  const std::string salt_1 =
      test_local_state()->GetString(prefs::kArcSerialNumberSalt);
  EXPECT_FALSE(salt_1.empty());
  EXPECT_EQ(kHexSaltLen, salt_1.size());

  // Calling the function again returns the same serial/salt.
  EXPECT_EQ(serialno_1, GetOrCreateSerialNumber(test_local_state(),
                                                chromeos_user, std::string()));
  EXPECT_EQ(salt_1, test_local_state()->GetString(prefs::kArcSerialNumberSalt));

  // A different user gets a different serial number, but the salt stays the
  // same.
  const std::string serialno_2 = GetOrCreateSerialNumber(
      test_local_state(), chromeos_user2, std::string());
  EXPECT_FALSE(serialno_2.empty());
  EXPECT_EQ(kSerialNumberLen, serialno_2.size());
  EXPECT_NE(serialno_1, serialno_2);
  EXPECT_EQ(salt_1, test_local_state()->GetString(prefs::kArcSerialNumberSalt));

  // Delete the salt in local state (which is what Chrome OS PowerWash does.)
  test_local_state()->ClearPref(prefs::kArcSerialNumberSalt);
  ASSERT_TRUE(
      test_local_state()->GetString(prefs::kArcSerialNumberSalt).empty());

  // Generate the salt and serial for |chromeos_user| again. Verify both are
  // different than the previous ones.
  const std::string serialno_3 =
      GetOrCreateSerialNumber(test_local_state(), chromeos_user, std::string());
  EXPECT_FALSE(serialno_3.empty());
  EXPECT_EQ(kSerialNumberLen, serialno_3.size());
  EXPECT_NE(serialno_1, serialno_3);

  const std::string salt_2 =
      test_local_state()->GetString(prefs::kArcSerialNumberSalt);
  EXPECT_FALSE(salt_2.empty());
  EXPECT_EQ(kHexSaltLen, salt_2.size());
  EXPECT_NE(salt_1, salt_2);

  // Delete the salt in local state again.
  test_local_state()->ClearPref(prefs::kArcSerialNumberSalt);
  ASSERT_TRUE(
      test_local_state()->GetString(prefs::kArcSerialNumberSalt).empty());

  // Pass |salt_on_disk| and verify hex-encoded version of the salt is stored
  // in local state.
  using std::literals::string_literals::operator""s;
  const std::string salt_on_disk = "BAADDECAFC0\0FFEE"s;
  const std::string salt_on_disk_hex = base::HexEncode(salt_on_disk);
  const std::string serialno_4 =
      GetOrCreateSerialNumber(test_local_state(), chromeos_user, salt_on_disk);
  EXPECT_FALSE(serialno_4.empty());
  EXPECT_EQ(kSerialNumberLen, serialno_4.size());
  EXPECT_NE(serialno_1, serialno_4);

  const std::string salt_3 =
      test_local_state()->GetString(prefs::kArcSerialNumberSalt);
  EXPECT_EQ(salt_on_disk_hex, salt_3);

  // A different user gets a different serial number, but the salt stays the
  // same. This time, pass a different salt on disk to verify it's ignored
  // when a salt already exists in local state.
  const std::string serialno_5 = GetOrCreateSerialNumber(
      test_local_state(), chromeos_user2,
      // Reverse |salt_on_disk| and pass it.
      std::string(salt_on_disk.rbegin(), salt_on_disk.rend()));
  EXPECT_FALSE(serialno_5.empty());
  EXPECT_EQ(kSerialNumberLen, serialno_5.size());
  EXPECT_NE(serialno_4, serialno_5);
  // Local state still has the non-reversed one.
  EXPECT_EQ(salt_on_disk_hex,
            test_local_state()->GetString(prefs::kArcSerialNumberSalt));
}

// That shouldn't happen, but verify that the function can recover the state
// even if local state has an invalid hex salt.
TEST_F(SerialNumberUtilTest, GetOrCreateSerialNumber_InvalidLocalState) {
  constexpr size_t kSaltLen = 16;
  const std::string chromeos_user = "user@gmail.com";

  // Manually set an invalid hex salt in local state, then call
  // GetOrCreateSerialNumber. Verify the local state is overwritten by a valid
  // one.
  const std::string invalid_hex_salt_1 = "THIS_IS_NOT_A_HEX_STRING";
  test_local_state()->SetString(prefs::kArcSerialNumberSalt,
                                invalid_hex_salt_1);
  EXPECT_FALSE(
      GetOrCreateSerialNumber(test_local_state(), chromeos_user, std::string())
          .empty());
  std::string salt = test_local_state()->GetString(prefs::kArcSerialNumberSalt);
  EXPECT_FALSE(salt.empty());
  EXPECT_NE(invalid_hex_salt_1, salt);

  // Do the same with a too short hex salt.
  const std::string buf(kSaltLen + 1, 'x');
  const std::string invalid_hex_salt_2 =
      base::HexEncode(buf.data(), kSaltLen - 1);  // too short
  test_local_state()->SetString(prefs::kArcSerialNumberSalt,
                                invalid_hex_salt_2);
  EXPECT_FALSE(
      GetOrCreateSerialNumber(test_local_state(), chromeos_user, std::string())
          .empty());
  salt = test_local_state()->GetString(prefs::kArcSerialNumberSalt);
  EXPECT_FALSE(salt.empty());
  EXPECT_NE(invalid_hex_salt_2, salt);

  // Do the same with a too long one.
  const std::string invalid_hex_salt_3 =
      base::HexEncode(buf.data(), kSaltLen + 1);  // too long
  test_local_state()->SetString(prefs::kArcSerialNumberSalt,
                                invalid_hex_salt_3);
  EXPECT_FALSE(
      GetOrCreateSerialNumber(test_local_state(), chromeos_user, std::string())
          .empty());
  salt = test_local_state()->GetString(prefs::kArcSerialNumberSalt);
  EXPECT_FALSE(salt.empty());
  EXPECT_NE(invalid_hex_salt_3, salt);

  // Test the valid case too.
  const std::string valid_hex_salt = base::HexEncode(buf.data(), kSaltLen);
  test_local_state()->SetString(prefs::kArcSerialNumberSalt, valid_hex_salt);
  EXPECT_FALSE(
      GetOrCreateSerialNumber(test_local_state(), chromeos_user, std::string())
          .empty());
  salt = test_local_state()->GetString(prefs::kArcSerialNumberSalt);
  EXPECT_FALSE(salt.empty());
  EXPECT_EQ(valid_hex_salt, salt);
}

// Verify that GetOrCreateSerialNumber uses decoded salt when computing the
// serial number.
TEST_F(SerialNumberUtilTest, GetOrCreateSerialNumber_SerialNumberComputation) {
  constexpr size_t kSaltLen = 16;
  const std::string chromeos_user = "user@gmail.com";

  // Set the |hex_salt| in local state.
  const std::string hex_salt = base::HexEncode(std::string(kSaltLen, 'x'));
  test_local_state()->SetString(prefs::kArcSerialNumberSalt, hex_salt);

  // Get a serial number based on the hex salt.
  const std::string serial_number =
      GetOrCreateSerialNumber(test_local_state(), chromeos_user, std::string());
  EXPECT_FALSE(serial_number.empty());

  // Directly compute the serial number with the *hex* salt (which
  // GetOrCreateSerialNumber is NOT supposed to do). Verify the returned
  // serial number is NOT the same as the one from GetOrCreateSerialNumber.
  EXPECT_NE(GenerateFakeSerialNumber(chromeos_user, hex_salt), serial_number);
}

// Tests that ReadSaltOnDisk can read a non-ASCII salt.
TEST_F(SerialNumberUtilTest, ReadSaltOnDisk) {
  constexpr int kSaltLen = 16;

  // Verify the function returns a non-null result when the file doesn't exist.
  std::optional<std::string> salt =
      ReadSaltOnDisk(base::FilePath("/nonexistent/path"));
  EXPECT_TRUE(salt.has_value());

  // Create a valid arc_salt file.
  using std::literals::string_literals::operator""s;
  const std::string expected_salt_value = "BAADDECAFC0\0FFEE"s;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath arc_salt_path = temp_dir.GetPath().Append("arc_salt");
  ASSERT_TRUE(base::WriteFile(arc_salt_path, expected_salt_value));

  // Verify the function can read the salt file even when the file contains
  // non-ASCII characters like '\0'.
  salt = ReadSaltOnDisk(arc_salt_path);
  ASSERT_TRUE(salt.has_value());
  EXPECT_EQ(expected_salt_value, salt.value());

  // Change the mode to drop the r bit. Verify the function returns false
  // when the file exists, but not readable.
  ASSERT_TRUE(base::SetPosixFilePermissions(arc_salt_path, 0300));
  salt = ReadSaltOnDisk(arc_salt_path);
  EXPECT_FALSE(salt.has_value());

  // Create a different salt file that has corrupted data. Verify the function
  // returns true but an empty |salt|.
  arc_salt_path = temp_dir.GetPath().Append("arc_salt2");
  ASSERT_TRUE(base::WriteFile(arc_salt_path,
                              std::string(kSaltLen - 1, 'x')));  // too short
  salt = ReadSaltOnDisk(arc_salt_path);
  ASSERT_TRUE(salt.has_value());
  EXPECT_TRUE(salt.value().empty());

  arc_salt_path = temp_dir.GetPath().Append("arc_salt3");
  ASSERT_TRUE(base::WriteFile(arc_salt_path,
                              std::string(kSaltLen + 1, 'x')));  // too long
  salt = ReadSaltOnDisk(arc_salt_path);
  ASSERT_TRUE(salt.has_value());
  EXPECT_TRUE(salt.value().empty());
}

}  // namespace

}  // namespace arc
