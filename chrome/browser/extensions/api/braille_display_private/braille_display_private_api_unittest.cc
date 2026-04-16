// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/braille_display_private_api.h"

#include "base/test/gtest_util.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace api {
namespace braille_display_private {

using BrailleDisplayPrivateApiUnittest = ExtensionServiceTestBase;

// Verifies that updateBluetoothBrailleDisplayAddress rejects a shell injection
// payload.
TEST_F(BrailleDisplayPrivateApiUnittest,
       UpdateBluetoothBrailleDisplayAddress_RejectsShellInjection) {
  InitializeEmptyExtensionService();
  auto function = base::MakeRefCounted<
      BrailleDisplayPrivateUpdateBluetoothBrailleDisplayAddressFunction>();
  EXPECT_EQ("Invalid Bluetooth address",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), R"(["; touch /tmp/pwned; #"])", profile()));
}

// Verifies that updateBluetoothBrailleDisplayAddress rejects a command
// substitution payload.
TEST_F(BrailleDisplayPrivateApiUnittest,
       UpdateBluetoothBrailleDisplayAddress_RejectsCommandSubstitution) {
  InitializeEmptyExtensionService();
  auto function = base::MakeRefCounted<
      BrailleDisplayPrivateUpdateBluetoothBrailleDisplayAddressFunction>();
  EXPECT_EQ("Invalid Bluetooth address",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), R"--(["$(reboot)"])--", profile()));
}

// Verifies that a valid address with an appended command is rejected.
TEST_F(BrailleDisplayPrivateApiUnittest,
       UpdateBluetoothBrailleDisplayAddress_RejectsAppendedCommand) {
  InitializeEmptyExtensionService();
  auto function = base::MakeRefCounted<
      BrailleDisplayPrivateUpdateBluetoothBrailleDisplayAddressFunction>();
  EXPECT_EQ("Invalid Bluetooth address",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), R"(["AA:BB:CC:DD:EE:FF; rm -rf /"])",
                profile()));
}

// Verifies that an empty string is rejected.
TEST_F(BrailleDisplayPrivateApiUnittest,
       UpdateBluetoothBrailleDisplayAddress_RejectsEmptyString) {
  InitializeEmptyExtensionService();
  auto function = base::MakeRefCounted<
      BrailleDisplayPrivateUpdateBluetoothBrailleDisplayAddressFunction>();
  EXPECT_EQ("Invalid Bluetooth address",
            api_test_utils::RunFunctionAndReturnError(function.get(),
                                                      R"([""])", profile()));
}

// Verifies that a non-hex string is rejected.
TEST_F(BrailleDisplayPrivateApiUnittest,
       UpdateBluetoothBrailleDisplayAddress_RejectsNonHex) {
  InitializeEmptyExtensionService();
  auto function = base::MakeRefCounted<
      BrailleDisplayPrivateUpdateBluetoothBrailleDisplayAddressFunction>();
  EXPECT_EQ("Invalid Bluetooth address",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), R"(["not-an-address"])", profile()));
}

// Verifies that a truncated address is rejected.
TEST_F(BrailleDisplayPrivateApiUnittest,
       UpdateBluetoothBrailleDisplayAddress_RejectsTruncatedAddress) {
  InitializeEmptyExtensionService();
  auto function = base::MakeRefCounted<
      BrailleDisplayPrivateUpdateBluetoothBrailleDisplayAddressFunction>();
  EXPECT_EQ("Invalid Bluetooth address",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), R"(["AA:BB:CC:DD:EE"])", profile()));
}

// Verifies that an overlong address is rejected.
TEST_F(BrailleDisplayPrivateApiUnittest,
       UpdateBluetoothBrailleDisplayAddress_RejectsOverlongAddress) {
  InitializeEmptyExtensionService();
  auto function = base::MakeRefCounted<
      BrailleDisplayPrivateUpdateBluetoothBrailleDisplayAddressFunction>();
  EXPECT_EQ("Invalid Bluetooth address",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), R"(["AA:BB:CC:DD:EE:FF:00"])", profile()));
}

// Verifies that CanonicalizeBluetoothAddress CHECKs on an invalid address,
// mirroring the defense-in-depth CHECK in RestartBrltty.
using BluetoothAddressDeathTest = testing::Test;

TEST_F(BluetoothAddressDeathTest, InvalidAddressCHECKFails) {
  EXPECT_CHECK_DEATH({
    std::string canonical =
        device::CanonicalizeBluetoothAddress("; touch /tmp/pwned; #");
    CHECK(!canonical.empty());
  });
}

// Verifies that valid Bluetooth MAC addresses pass validation via
// CanonicalizeBluetoothAddress (the same function used by the API and
// RestartBrltty).
TEST(BluetoothAddressValidationTest, AcceptsColonSeparated) {
  EXPECT_FALSE(
      device::CanonicalizeBluetoothAddress("AA:BB:CC:DD:EE:FF").empty());
}

TEST(BluetoothAddressValidationTest, AcceptsLowercase) {
  EXPECT_FALSE(
      device::CanonicalizeBluetoothAddress("aa:bb:cc:dd:ee:ff").empty());
}

TEST(BluetoothAddressValidationTest, AcceptsDashSeparated) {
  EXPECT_FALSE(
      device::CanonicalizeBluetoothAddress("AA-BB-CC-DD-EE-FF").empty());
}

TEST(BluetoothAddressValidationTest, AcceptsNoSeparators) {
  EXPECT_FALSE(
      device::CanonicalizeBluetoothAddress("AABBCCDDEEFF").empty());
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions
