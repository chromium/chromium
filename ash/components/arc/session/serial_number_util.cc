// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/serial_number_util.h"

#include <string_view>

#include "ash/components/arc/arc_prefs.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/random.h"
#include "crypto/sha2.h"

namespace arc {

namespace {

constexpr const size_t kArcSaltFileSize = 16;

// Returns true if the hex-encoded salt in Local State is valid.
bool IsValidHexSalt(std::string_view hex_salt) {
  std::string salt;
  if (!base::HexStringToString(hex_salt, &salt)) {
    LOG(WARNING) << "Not a hex string: " << hex_salt;
    return false;
  }
  if (salt.size() != kArcSaltFileSize) {
    LOG(WARNING) << "Salt size invalid: " << salt.size();
    return false;
  }
  return true;
}

}  // namespace

std::string GenerateFakeSerialNumber(std::string_view chromeos_user,
                                     std::string_view salt) {
  constexpr size_t kMaxHardwareIdLen = 20;
  std::string input(chromeos_user);
  input.append(salt.begin(), salt.end());
  const std::string hash(crypto::SHA256HashString(input));
  return base::HexEncode(hash).substr(0, kMaxHardwareIdLen);
}

std::string GetOrCreateSerialNumber(PrefService* local_state,
                                    std::string_view chromeos_user,
                                    std::string_view arc_salt_on_disk) {
  DCHECK(local_state);
  DCHECK(!chromeos_user.empty());

  std::string hex_salt = local_state->GetString(prefs::kArcSerialNumberSalt);
  if (hex_salt.empty() || !IsValidHexSalt(hex_salt)) {
    // This path is taken 1) on the very first ARC boot, 2) on the first boot
    // after powerwash, 3) on the first boot after upgrading to ARCVM, or 4)
    // when the salt in local state is corrupted.
    if (arc_salt_on_disk.empty()) {
      // The device doesn't have the salt file for ARC container. Create it from
      // scratch in the same way as ARC container.
      uint8_t rand_value[kArcSaltFileSize];
      crypto::RandBytes(rand_value);
      hex_salt = base::HexEncode(rand_value);
    } else {
      // The device has the one for container. Reuse it for ARCVM.
      DCHECK_EQ(kArcSaltFileSize, arc_salt_on_disk.size());
      hex_salt = base::HexEncode(arc_salt_on_disk);
    }
    local_state->SetString(prefs::kArcSerialNumberSalt, hex_salt);
  }

  // We store hex-encoded version of the salt in the local state, but to compute
  // the serial number, we use the decoded version to be compatible with the
  // arc-setup code for P.
  std::string decoded_salt;
  const bool result = base::HexStringToString(hex_salt, &decoded_salt);
  DCHECK(result) << hex_salt;
  return GenerateFakeSerialNumber(chromeos_user, decoded_salt);
}

std::optional<std::string> ReadSaltOnDisk(const base::FilePath& salt_path) {
  if (!base::PathExists(salt_path)) {
    VLOG(2) << "ARC salt file doesn't exist: " << salt_path;
    return std::string();
  }
  std::string salt;
  if (!base::ReadFileToString(salt_path, &salt)) {
    PLOG(ERROR) << "Failed to read " << salt_path;
    return std::nullopt;
  }
  if (salt.size() != kArcSaltFileSize) {
    LOG(WARNING) << "Ignoring invalid ARC salt on disk. size=" << salt.size();
    salt.clear();
  }
  VLOG(1) << "Successfully read ARC salt on disk: " << salt_path;
  return salt;
}

}  // namespace arc
