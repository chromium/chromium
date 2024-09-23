// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_SERIAL_NUMBER_UTIL_H_
#define ASH_COMPONENTS_ARC_SESSION_SERIAL_NUMBER_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "components/prefs/pref_service.h"

namespace arc {

// Generates a unique, 20-character hex string from |chromeos_user| and
// |salt| which can be used as Android's ro.boot.serialno and ro.serialno
// properties. Note that Android treats serialno in a case-insensitive manner.
// |salt| cannot be the hex-encoded one.
// Note: The function must be the exact copy of the one in platform2/arc/setup/.
std::string GenerateFakeSerialNumber(std::string_view chromeos_user,
                                     std::string_view salt);

// Generates and returns a serial number from the salt in |local_state| and
// |chromeos_user|. When |local_state| does not have it (or has a corrupted
// one), this function creates a new random salt. When creates it, the function
// copies |arc_salt_on_disk| to |local_state| if |arc_salt_on_disk| is not
// empty.
std::string GetOrCreateSerialNumber(PrefService* local_state,
                                    std::string_view chromeos_user,
                                    std::string_view arc_salt_on_disk);

// Reads a salt from |salt_path| and returns it. Returns a non-null value when
// the file read is successful or the file does not exist.
std::optional<std::string> ReadSaltOnDisk(const base::FilePath& salt_path);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_SERIAL_NUMBER_UTIL_H_
