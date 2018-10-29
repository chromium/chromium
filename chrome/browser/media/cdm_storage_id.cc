// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_storage_id.h"

#include "base/callback.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/media/cdm_storage_id_key.h"
#include "chrome/browser/media/media_storage_id_salt.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "media/media_buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "base/bind.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
#if BUILDFLAG(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#else
#error "RLZ must be enabled on Windows/Mac"
#endif  // BUILDFLAG(ENABLE_RLZ)
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

namespace {

// Calculates the Storage Id based on:
//   |storage_id_key| - a browser identifier
//   |profile_salt|   - setting in the user's profile
//   |origin|         - the origin used
//   |machine_id|     - a device identifier
// If all the parameters appear valid, this function returns the SHA256
// checksum of the above values. If any of the values are invalid, the empty
// vector is returned.
std::vector<uint8_t> CalculateStorageId(
    const std::string& storage_id_key,
    const std::vector<uint8_t>& profile_salt,
    const url::Origin& origin,
    const std::string& machine_id) {
  if (storage_id_key.length() < kMinimumCdmStorageIdKeyLength) {
    DLOG(ERROR) << "Storage key not set correctly, length: "
                << storage_id_key.length();
    return {};
  }

  if (profile_salt.size() != MediaStorageIdSalt::kSaltLength) {
    DLOG(ERROR) << "Unexpected salt bytes length: " << profile_salt.size();
    return {};
  }

  if (origin.opaque()) {
    DLOG(ERROR) << "Unexpected origin: " << origin;
    return {};
  }

  if (machine_id.empty()) {
    DLOG(ERROR) << "Empty machine id";
    return {};
  }

  // Build the identifier as follows:
  // SHA256(machine_id + origin + storage_id_key + profile_salt)
  std::string origin_str = origin.Serialize();
  std::unique_ptr<crypto::SecureHash> sha256 =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  sha256->Update(machine_id.data(), machine_id.length());
  sha256->Update(origin_str.data(), origin_str.length());
  sha256->Update(storage_id_key.data(), storage_id_key.length());
  sha256->Update(profile_salt.data(), profile_salt.size());

  std::vector<uint8_t> result(crypto::kSHA256Length);
  sha256->Finish(result.data(), result.size());
  return result;
}

#if defined(OS_CHROMEOS)
void ComputeAndReturnStorageId(const std::vector<uint8_t>& profile_salt,
                               const url::Origin& origin,
                               CdmStorageIdCallback callback,
                               const std::string& machine_id) {
  std::string storage_id_key = GetCdmStorageIdKey();
  std::move(callback).Run(
      CalculateStorageId(storage_id_key, profile_salt, origin, machine_id));
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

void ComputeStorageId(const std::vector<uint8_t>& profile_salt,
                      const url::Origin& origin,
                      CdmStorageIdCallback callback) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  std::string machine_id;
  std::string storage_id_key = GetCdmStorageIdKey();
  rlz_lib::GetMachineId(&machine_id);
  std::move(callback).Run(
      CalculateStorageId(storage_id_key, profile_salt, origin, machine_id));

#elif defined(OS_CHROMEOS)
  CdmStorageIdCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                  std::vector<uint8_t>());
  chromeos::SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&ComputeAndReturnStorageId, profile_salt, origin,
                 base::Passed(&scoped_callback)));

#else
#error Storage ID enabled but not implemented for this platform.
#endif
}

