// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/android_rsa.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string_view>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "crypto/keypair.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace {

// The Android RSA format is fixed-width and can only represent 2048-bit RSA.
constexpr size_t kRSAModulusBytes = 2048 / 8;

// The Android RSA format is 524 bytes in total:
// - 4 bytes, little-endian: length of n in number of u32s, must be 64
// - 4 bytes, little-endian: precomputed -1 / n[0] mod 2^32 (unused in modern
//   Android)
// - 256 bytes, little-endian: modulus
// - 256 bytes, little-endian: precomputed R^2 (unused in modern Android)
// - 4 bytes, little-endian: public exponent
constexpr size_t kAndroidRSASize = 524;

// http://en.wikipedia.org/wiki/Extended_Euclidean_algorithm
// a * x + b * y = gcd(a, b) = d
void ExtendedEuclid(uint64_t a,
                    uint64_t b,
                    uint64_t* x,
                    uint64_t* y,
                    uint64_t* d) {
  uint64_t x1 = 0, x2 = 1, y1 = 1, y2 = 0;

  while (b > 0) {
    uint64_t q = a / b;
    uint64_t r = a % b;
    *x = x2 - q * x1;
    *y = y2 - q * y1;
    a = b;
    b = r;
    x2 = x1;
    x1 = *x;
    y2 = y1;
    y1 = *y;
  }

  *d = a;
  *x = x2;
  *y = y2;
}

uint32_t ModInverse2_32(uint32_t a) {
  CHECK_EQ(a & 1u, 1u);  // a must be odd
  uint64_t d, x, y;
  ExtendedEuclid(a, 0x100000000, &x, &y, &d);
  CHECK_EQ(d, 1u);  // If a is odd, there is an inverse.
  return static_cast<uint32_t>(x);
}

bool WriteLittleEndianBignum(const BIGNUM* bn, base::span<uint8_t> out) {
  return BN_bn2le_padded(out.data(), out.size(), bn);
}

}  // namespace

crypto::keypair::PrivateKey AndroidRSAPrivateKey(Profile* profile) {
  std::string encoded_key =
      profile->GetPrefs()->GetString(prefs::kDevToolsAdbKey);
  std::string decoded_key;
  std::optional<crypto::keypair::PrivateKey> key;
  if (!encoded_key.empty() && base::Base64Decode(encoded_key, &decoded_key)) {
    key = crypto::keypair::PrivateKey::FromPrivateKeyInfo(
        base::as_byte_span(decoded_key));
  }
  if (!key) {
    key = crypto::keypair::PrivateKey::GenerateRsa2048();
    profile->GetPrefs()->SetString(prefs::kDevToolsAdbKey,
                                   base::Base64Encode(key->ToPrivateKeyInfo()));
  }
  return *key;
}

std::optional<std::string> AndroidRSAPublicKey(
    crypto::keypair::PrivateKey key) {
  // Assemble Android's custom RSA format. This format dates to when Android was
  // using a custom "minicrypt" RSA implementation and was just minicrypt's
  // in-memory representation. The format assumes 2048-bit RSA (up to byte
  // precision) and also includes precomputed information for Montgomery
  // reduction with 32-bit words.
  //
  // This precomputed information no longer makes sense with modern 64-bit
  // processors, and does not contain quite enough information for 64-bit
  // Montgomery reduction. Starting Android O, it no longer looks at it at all.
  // See https://r.android.com/212780 and https://r.android.com/212781.
  //
  // However, that information is still hashed into existing ADB key
  // fingerprints, so continue computing them to keep the fingerprint stable.
  RSA* rsa = EVP_PKEY_get0_RSA(key.key());
  uint64_t e;
  if (RSA_size(rsa) != kRSAModulusBytes ||  //
      !BN_get_u64(RSA_get0_e(rsa), &e) ||
      !base::IsValueInRangeForNumericType<uint32_t>(e)) {
    return std::nullopt;
  }

  std::array<uint8_t, kAndroidRSASize> out;
  auto writer = base::SpanWriter(base::span(out));
  writer.WriteU32LittleEndian(kRSAModulusBytes / 4);
  // Reserve space for ninv. We'll compute it after we've written N.
  auto ninv = *writer.Skip<4>();
  auto n = *writer.Skip<kRSAModulusBytes>();
  CHECK(WriteLittleEndianBignum(RSA_get0_n(rsa), n));
  // Fill in RR, or 2^4096 mod N.
  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  bssl::UniquePtr<BIGNUM> rr(BN_new());
  CHECK(BN_set_bit(rr.get(), 4096));
  CHECK(BN_mod(rr.get(), rr.get(), RSA_get0_n(rsa), ctx.get()));
  CHECK(WriteLittleEndianBignum(rr.get(), *writer.Skip<kRSAModulusBytes>()));
  writer.WriteU32LittleEndian(base::checked_cast<uint32_t>(e));

  // Compute ninv = -1 / n[0] mod 2^32. This value being 32-bit makes the
  // pre-computed Montgomery values useless on a modern system. A 64-bit
  // Montgomery reduction needs it mod 2^64. But we compute it anyway. The mod
  // inverse cannot fail because BoringSSL will ensure N is odd.
  uint32_t n0 = base::U32FromLittleEndian(n.first<4>());
  ninv.copy_from(base::U32ToLittleEndian(0u - ModInverse2_32(n0)));

  // Make sure we've written everything.
  CHECK_EQ(writer.remaining(), 0u);
  return base::Base64Encode(out);
}

std::string AndroidRSASign(crypto::keypair::PrivateKey key,
                           const std::string& body) {
  RSA* rsa = EVP_PKEY_get0_RSA(key.key());
  if (!rsa) {
    return std::string();
  }

  std::string result(RSA_size(rsa), 0);
  unsigned int len = 0;

  auto body_bytes = base::as_byte_span(body);
  auto result_bytes = base::as_writable_byte_span(result);

  // The ADB protocol requires us to sign a 20-byte challenge, and assumes the
  // challenge is a pre-hashed SHA-1 digest, although there is no guarantee that
  // that is true, and signs it without further hashing. In general this is not
  // a secure signature scheme and should not be used elsewhere.
  if (!RSA_sign(NID_sha1, body_bytes.data(), body_bytes.size(),
                result_bytes.data(), &len, rsa)) {
    return std::string();
  }
  result.resize(len);
  return result;
}
