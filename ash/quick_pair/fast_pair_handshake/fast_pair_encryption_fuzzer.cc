// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_encryption.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>
#include <array>

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_key_pair.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "chromeos/ash/services/quick_pair/fast_pair_decryption.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/nid.h"

namespace {

constexpr size_t kXSize = 32;
constexpr size_t kYSize = 1;
constexpr size_t kKeySize = /*type byte=*/1 + /*x coord=*/32 + /*y coord=*/32;

struct Environment {
  Environment() {
    // Disable noisy logging for fuzzing.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }

  ash::quick_pair::ScopedDisableLoggingForTesting disable_logging_;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Enforce a minimum input size so that we can pass in valid parameters
  // to EncryptBytes and GenerateKeysWithEcdhKeyAgreement.
  if (size < 2 * ash::quick_pair::fast_pair_decryption::kBlockByteSize)
    return 0;

  static base::NoDestructor<Environment> env;
  FuzzedDataProvider fuzzed_data(data, size);

  std::vector<uint8_t> aes_key_bytes = fuzzed_data.ConsumeBytes<uint8_t>(
      ash::quick_pair::fast_pair_decryption::kBlockByteSize);
  std::array<uint8_t, ash::quick_pair::fast_pair_decryption::kBlockByteSize>
      aes_key_arr;
  std::copy_n(aes_key_bytes.begin(),
              ash::quick_pair::fast_pair_decryption::kBlockByteSize,
              aes_key_arr.begin());

  std::vector<uint8_t> data_bytes = fuzzed_data.ConsumeBytes<uint8_t>(
      ash::quick_pair::fast_pair_decryption::kBlockByteSize);
  std::array<uint8_t, ash::quick_pair::fast_pair_decryption::kBlockByteSize>
      data_arr;
  std::copy_n(data_bytes.begin(),
              ash::quick_pair::fast_pair_decryption::kBlockByteSize,
              data_arr.begin());

  ash::quick_pair::fast_pair_encryption::EncryptBytes(aes_key_arr, data_arr);

  // In order to fuzz a valid EC_POINT, the fuzz needs to have at least
  // kXSize + kYSize bytes remaining. For simplicity, exit early if there
  // are not exactly as many bytes as required.
  if (size != 2 * ash::quick_pair::fast_pair_decryption::kBlockByteSize +
                  kXSize + kYSize) {
    std::string invalid_len_string = fuzzed_data.ConsumeRandomLengthString();

    ash::quick_pair::fast_pair_encryption::GenerateKeysWithEcdhKeyAgreement(
        invalid_len_string);

    return 0;
  }

  // Generates a random point on the curve defined by EC_GROUP.
  bssl::UniquePtr<EC_GROUP> ec_group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(ec_group.get()));

  // Set x value and y bit according to fuzz.
  auto x_value_arr = fuzzed_data.ConsumeBytes<uint8_t>(kXSize);
  DCHECK(x_value_arr.size() == kXSize);
  bssl::UniquePtr<BIGNUM> x(BN_new());
  DCHECK(BN_le2bn(&x_value_arr[0], kXSize, x.get()));

  auto y_value_arr = fuzzed_data.ConsumeBytes<uint8_t>(kYSize);
  DCHECK(y_value_arr.size() == kYSize);
  int y_bit = y_value_arr[0] & 1;

  // Set EC_POINT according to the compressed coordinates x and y_bit. This
  // effectively uses fuzz to generate a point on the EC without us having to
  // explicitly compute the solution y for x on the EC. Fails 50% of the time
  // when generated x value has no solution on the EC.
  if (!EC_POINT_set_compressed_coordinates_GFp(ec_group.get(), point.get(),
                                               x.get(), y_bit, /*ctx=*/nullptr))
    return 0;

  // Convert compressed EC_POINT into the uncompressed string expected by the
  // function.
  std::array<uint8_t, kKeySize> buffer;
  DCHECK(EC_POINT_point2oct(ec_group.get(), point.get(),
                            POINT_CONVERSION_UNCOMPRESSED, buffer.data(),
                            kKeySize, /*ctx=*/nullptr));
  // Function expects a string which is missing the first type byte.
  std::string anti_spoofing_key(buffer.data() + 1, buffer.data() + kKeySize);
  DCHECK(anti_spoofing_key.length() == kKeySize - 1);

  ash::quick_pair::fast_pair_encryption::GenerateKeysWithEcdhKeyAgreement(
      anti_spoofing_key);

  return 0;
}
