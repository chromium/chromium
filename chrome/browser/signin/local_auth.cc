// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/local_auth.h"

#include <memory>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/os_crypt/os_crypt.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "crypto/random.h"
#include "crypto/secure_util.h"
#include "crypto/symmetric_key.h"

namespace {

struct HashEncoding {
  char version;
  unsigned hash_bits;
  unsigned hash_bytes;
  unsigned iteration_count;
  unsigned stored_bits;
  unsigned stored_bytes;

  public:
   HashEncoding(char version,
                unsigned hash_bits,
                unsigned hash_bytes,
                unsigned iteration_count,
                unsigned stored_bits,
                unsigned stored_bytes) :
      version(version),
      hash_bits(hash_bits),
      hash_bytes(hash_bytes),
      iteration_count(iteration_count),
      stored_bits(stored_bits),
      stored_bytes(stored_bytes) {}
};

// WARNING: Changing these values will make it impossible to do off-line
// authentication until the next successful on-line authentication.  To change
// these safely, add a new HashEncoding object below and increment
// NUM_HASH_ENCODINGS.
const char kHash1Version = '1';
const unsigned kHash1Bits = 256;
const unsigned kHash1Bytes = kHash1Bits / 8;
const unsigned kHash1IterationCount = 100000;

// Store 13 bits to provide pin-like security (8192 possible values), without
// providing a complete oracle for the user's GAIA password.
const char kHash2Version = '2';
const unsigned kHash2Bits = 256;
const unsigned kHash2Bytes = kHash2Bits / 8;
const unsigned kHash2IterationCount = 100000;
const unsigned kHash2StoredBits = 13;
const unsigned kHash2StoredBytes = (kHash2StoredBits + 7) / 8;

const int NUM_HASH_ENCODINGS = 2;
HashEncoding encodings[NUM_HASH_ENCODINGS] = {
  HashEncoding(
      kHash1Version, kHash1Bits, kHash1Bytes, kHash1IterationCount, 0, 0),
  HashEncoding(
      kHash2Version, kHash2Bits, kHash2Bytes, kHash2IterationCount,
          kHash2StoredBits, kHash2StoredBytes)
};

const HashEncoding* GetEncodingForVersion(char version) {
  // Note that versions are 1-indexed.
  DCHECK(version > '0' && version <= ('0' + NUM_HASH_ENCODINGS));
  return &encodings[(version - '0') - 1];
}

std::string TruncateStringByBits(const std::string& str,
                                 const size_t len_bits) {
  if (len_bits % 8 == 0)
    return str.substr(0, len_bits / 8);

  // The initial truncation copies whole bytes
  int number_bytes = (len_bits + 7) / 8;
  std::string truncated_string = str.substr(0, number_bytes);

  // Keep the prescribed number of bits from the last byte.
  unsigned last_char_bitmask = (1 << (len_bits % 8)) - 1;
  truncated_string[number_bytes - 1] &= last_char_bitmask;
  return truncated_string;
}

std::string CreateSecurePasswordHash(const std::string& salt,
                                     const std::string& password,
                                     const HashEncoding& encoding) {
  DCHECK_EQ(encoding.hash_bytes, salt.length());
  base::Time start_time = base::Time::Now();

  // Library call to create secure password hash as SymmetricKey (uses PBKDF2).
  std::unique_ptr<crypto::SymmetricKey> password_key(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES, password, salt, encoding.iteration_count,
          encoding.hash_bits));
  std::string password_hash = password_key->key();
  DCHECK_EQ(encoding.hash_bytes, password_hash.length());

  UMA_HISTOGRAM_TIMES("PasswordHash.CreateTime",
                      base::Time::Now() - start_time);

  if (encoding.stored_bits) {
    password_hash = TruncateStringByBits(password_hash, encoding.stored_bits);
    DCHECK_EQ(encoding.stored_bytes, password_hash.length());
  }
  DCHECK_EQ(encoding.stored_bytes ? encoding.stored_bytes : encoding.hash_bytes,
            password_hash.length());
  return password_hash;
}

std::string EncodePasswordHashRecord(const std::string& record,
                                     const HashEncoding& encoding) {
  // Encrypt the hash using the OS account-password protection (if available).
  std::string encoded;
  const bool success = OSCrypt::EncryptString(record, &encoded);
  DCHECK(success);

  // Convert binary record to text for preference database.
  std::string encoded64;
  base::Base64Encode(encoded, &encoded64);

  // Stuff the "encoding" value into the first byte.
  encoded64.insert(0, &encoding.version, sizeof(encoding.version));

  return encoded64;
}

bool DecodePasswordHashRecord(const std::string& encoded,
                              std::string* decoded,
                              char* encoding) {
  // Extract the "encoding" value from the first byte and validate.
  if (encoded.length() < 1)
    return false;
  *encoding = encoded[0];
  if (!GetEncodingForVersion(*encoding))
    return false;

  // Stored record is base64; convert to binary.
  std::string unbase64;
  if (!base::Base64Decode(encoded.substr(1), &unbase64))
    return false;

  // Decrypt the record using the OS account-password protection (if available).
  return OSCrypt::DecryptString(unbase64, decoded);
}

}  // namespace

std::string LocalAuth::TruncateStringByBits(const std::string& str,
                                            const size_t len_bits) {
  return ::TruncateStringByBits(str, len_bits);
}

void LocalAuth::SetLocalAuthCredentialsWithEncoding(
    ProfileAttributesEntry* entry,
    const std::string& password,
    char encoding_version) {
  const HashEncoding& encoding = encodings[(encoding_version - '0') - 1];

  // Salt should be random data, as long as the hash length, and different with
  // every save.
  std::string salt_str;
  crypto::RandBytes(base::WriteInto(&salt_str, encoding.hash_bytes + 1),
                    encoding.hash_bytes);

  // Perform secure hash of password for storage.
  std::string password_hash = CreateSecurePasswordHash(
      salt_str, password, encoding);

  // Group all fields into a single record for storage;
  std::string record;
  record.append(salt_str);
  record.append(password_hash);

  // Encode it and store it.
  std::string encoded = EncodePasswordHashRecord(record, encoding);
  entry->SetLocalAuthCredentials(encoded);
}

void LocalAuth::SetLocalAuthCredentials(ProfileAttributesEntry* entry,
                                        const std::string& password) {
  DCHECK(entry);
  DCHECK(password.length());
  SetLocalAuthCredentialsWithEncoding(
      entry, password, '0' + NUM_HASH_ENCODINGS);
}

void LocalAuth::SetLocalAuthCredentials(const Profile* profile,
                                        const std::string& password) {
  DCHECK(g_browser_process->profile_manager()->IsValidProfile(profile));
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  DCHECK(entry);
  SetLocalAuthCredentials(entry, password);
}

bool LocalAuth::ValidateLocalAuthCredentials(ProfileAttributesEntry* entry,
                                             const std::string& password) {
  DCHECK(entry);

  std::string record;
  char encoding;

  std::string encodedhash = entry->GetLocalAuthCredentials();
  if (encodedhash.length() == 0 && password.length() == 0)
    return true;
  if (!DecodePasswordHashRecord(encodedhash, &record, &encoding))
    return false;

  std::string password_hash;
  const char* password_saved;
  const char* password_check;
  size_t password_length;

  const HashEncoding* hash_encoding = GetEncodingForVersion(encoding);
  if (!hash_encoding) {
    // Unknown encoding.
    return false;
  }

  // Extract salt.
  std::string salt_str(record.data(), hash_encoding->hash_bytes);
  // Extract password.
  password_saved = record.data() + hash_encoding->hash_bytes;
  password_hash = CreateSecurePasswordHash(salt_str, password, *hash_encoding);
  password_length = hash_encoding->stored_bytes;
  password_check = password_hash.data();

  bool passwords_match = crypto::SecureMemEqual(
      password_saved, password_check, password_length);

  // Update the stored credentials to the latest encoding if necessary.
  if (passwords_match && (hash_encoding->version - '0') != NUM_HASH_ENCODINGS)
    SetLocalAuthCredentials(entry, password);
  return passwords_match;
}

bool LocalAuth::ValidateLocalAuthCredentials(const Profile* profile,
                                             const std::string& password) {
  DCHECK(g_browser_process->profile_manager()->IsValidProfile(profile));
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());

  if (!entry) {
    NOTREACHED();
    return false;
  }

  return ValidateLocalAuthCredentials(entry, password);
}
