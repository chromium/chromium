// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/devtools/device/usb/android_rsa.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <limits>
#include <memory>
#include <numeric>
#include <string_view>

#include "base/base64.h"
#include "base/containers/span.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "crypto/rsa_private_key.h"
#include "crypto/signature_creator.h"
#include "net/cert/asn1_util.h"

namespace {

const size_t kRSANumWords = 64;
const size_t kBigIntSize = 1024;

static const char kDummyRSAPublicKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA6OSJ64q+ZLg7VV2ojEPh5TRbYjwbT"
    "TifSPeFIV45CHnbTWYiiIn41wrozpYizNsMWZUBjdah1N78WVhbyDrnr0bDgFp+gXjfVppa3I"
    "gjiohEcemK3omXi3GDMK8ERhriLUKfQS842SXtQ8I+KoZtpCkGM//0h7+P+Rhm0WwdipIRMhR"
    "8haNAeyDiiCvqJcvevv2T52vqKtS3aWz+GjaTJJLVWydEpz9WdvWeLfFVhe2ZnqwwZNa30Qoj"
    "fsnvjaMwK2MU7uYfRBPuvLyK5QESWBpArNDd6ULl8Y+NU6kwNOVDc87OASCVEM1gw2IMi2mo2"
    "WO5ywp0UWRiGZCkK+wOFQIDAQAB";

typedef struct RSAPublicKey {
  int len;                    // Length of n[] in number of uint32_t
  uint32_t n0inv;             // -1 / n[0] mod 2^32
  uint32_t n[kRSANumWords];   // modulus as little endian array
  uint32_t rr[kRSANumWords];  // R^2 as little endian array
  int exponent;               // 3 or 65537
} RSAPublicKey;

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

uint32_t ModInverse(uint64_t a, uint64_t m) {
  uint64_t d, x, y;
  ExtendedEuclid(a, m, &x, &y, &d);
  if (d == 1)
    return static_cast<uint32_t>(x);
  return 0;
}

uint32_t* BnNew() {
  uint32_t* result = new uint32_t[kBigIntSize];
  memset(result, 0, kBigIntSize * sizeof(uint32_t));
  return result;
}

void BnFree(uint32_t* a) {
  delete[] a;
}

uint32_t* BnCopy(uint32_t* a) {
  uint32_t* result = new uint32_t[kBigIntSize];
  memcpy(result, a, kBigIntSize * sizeof(uint32_t));
  return result;
}

uint32_t* BnMul(uint32_t* a, uint32_t b) {
  uint32_t* result = BnNew();
  uint64_t carry_over = 0;
  for (size_t i = 0; i < kBigIntSize; ++i) {
    carry_over += static_cast<uint64_t>(a[i]) * b;
    result[i] = carry_over & std::numeric_limits<uint32_t>::max();
    carry_over >>= 32;
  }
  return result;
}

void BnSub(uint32_t* a, uint32_t* b) {
  int carry_over = 0;
  for (size_t i = 0; i < kBigIntSize; ++i) {
    int64_t sub = static_cast<int64_t>(a[i]) - b[i] - carry_over;
    carry_over = 0;
    if (sub < 0) {
      carry_over = 1;
      sub += 0x100000000LL;
    }
    a[i] = static_cast<uint32_t>(sub);
  }
}

void BnLeftShift(uint32_t* a, int offset) {
  for (int i = kBigIntSize - offset - 1; i >= 0; --i)
    a[i + offset] = a[i];
  for (int i = 0; i < offset; ++i)
    a[i] = 0;
}

int BnCompare(uint32_t* a, uint32_t* b) {
  for (int i = kBigIntSize - 1; i >= 0; --i) {
    if (a[i] > b[i])
      return 1;
    if (a[i] < b[i])
      return -1;
  }
  return 0;
}

uint64_t BnGuess(uint32_t* a, uint32_t* b, uint64_t from, uint64_t to) {
  if (from + 1 >= to)
    return from;

  uint64_t guess = std::midpoint(from, to);
  uint32_t* t = BnMul(b, static_cast<uint32_t>(guess));
  int result = BnCompare(a, t);
  BnFree(t);
  if (result > 0)
    return BnGuess(a, b, guess, to);
  if (result < 0)
    return BnGuess(a, b, from, guess);
  return guess;
}

void BnDiv(uint32_t* a, uint32_t* b, uint32_t** pq, uint32_t** pr) {
  if (BnCompare(a, b) < 0) {
    if (pq)
      *pq = BnNew();
    if (pr)
      *pr = BnCopy(a);
    return;
  }

  int oa = kBigIntSize - 1;
  int ob = kBigIntSize - 1;
  for (; oa > 0 && !a[oa]; --oa) {}
  for (; ob > 0 && !b[ob]; --ob) {}
  uint32_t* q = BnNew();
  uint32_t* ca = BnCopy(a);

  int digit = a[oa] < b[ob] ? oa - ob - 1 : oa - ob;

  for (; digit >= 0; --digit) {
    uint32_t* shifted_b = BnCopy(b);
    BnLeftShift(shifted_b, digit);
    uint32_t value = static_cast<uint32_t>(BnGuess(
        ca, shifted_b, 0,
        static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1));
    q[digit] = value;
    uint32_t* t = BnMul(shifted_b, value);
    BnSub(ca, t);
    BnFree(t);
    BnFree(shifted_b);
  }

  if (pq)
    *pq = q;
  else
    BnFree(q);
  if (pr)
    *pr = ca;
  else
    BnFree(ca);
}

}  // namespace

std::unique_ptr<crypto::RSAPrivateKey> AndroidRSAPrivateKey(Profile* profile) {
  std::string encoded_key =
      profile->GetPrefs()->GetString(prefs::kDevToolsAdbKey);
  std::string decoded_key;
  std::unique_ptr<crypto::RSAPrivateKey> key;
  if (!encoded_key.empty() && base::Base64Decode(encoded_key, &decoded_key)) {
    std::vector<uint8_t> key_info(decoded_key.begin(), decoded_key.end());
    key = crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_info);
  }
  if (!key) {
    key = crypto::RSAPrivateKey::Create(2048);
    std::vector<uint8_t> key_info;
    if (!key || !key->ExportPrivateKey(&key_info))
      return nullptr;

    std::string key_string(key_info.begin(), key_info.end());
    encoded_key = base::Base64Encode(key_string);
    profile->GetPrefs()->SetString(prefs::kDevToolsAdbKey,
                                   encoded_key);
  }
  return key;
}

std::string AndroidRSAPublicKey(crypto::RSAPrivateKey* key) {
  std::vector<uint8_t> public_key;
  if (!key)
    return kDummyRSAPublicKey;

  key->ExportPublicKey(&public_key);
  std::string asn1(public_key.begin(), public_key.end());

  std::string_view pk;
  if (!net::asn1::ExtractSubjectPublicKeyFromSPKI(asn1, &pk))
    return kDummyRSAPublicKey;

  // Skip 10 byte asn1 prefix to the modulus.
  std::vector<uint8_t> pk_data(pk.data() + 10, pk.data() + pk.length());
  uint32_t* n = BnNew();
  for (size_t i = 0; i < kRSANumWords; ++i) {
    uint32_t t = pk_data[4 * i];
    t = t << 8;
    t += pk_data[4 * i + 1];
    t = t << 8;
    t += pk_data[4 * i + 2];
    t = t << 8;
    t += pk_data[4 * i + 3];
    n[kRSANumWords - i - 1] = t;
  }
  uint64_t n0 = n[0];

  RSAPublicKey pkey;
  pkey.len = kRSANumWords;
  pkey.exponent = 65537; // Fixed public exponent
  pkey.n0inv = 0 - ModInverse(n0, 0x100000000LL);
  if (pkey.n0inv == 0)
    return kDummyRSAPublicKey;

  uint32_t* r = BnNew();
  r[kRSANumWords * 2] = 1;

  uint32_t* rr;
  BnDiv(r, n, nullptr, &rr);

  for (size_t i = 0; i < kRSANumWords; ++i) {
    pkey.n[i] = n[i];
    pkey.rr[i] = rr[i];
  }

  BnFree(n);
  BnFree(r);
  BnFree(rr);

  return base::Base64Encode(base::byte_span_from_ref(pkey));
}

std::string AndroidRSASign(crypto::RSAPrivateKey* key,
                           const std::string& body) {
  std::vector<uint8_t> digest(body.begin(), body.end());
  std::vector<uint8_t> result;
  if (!crypto::SignatureCreator::Sign(key, crypto::SignatureCreator::SHA1,
                                      digest.data(), digest.size(), &result)) {
    return std::string();
  }
  return std::string(result.begin(), result.end());
}
