// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_signer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/to_value_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "crypto/random.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "extensions/common/extension.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "rlz/buildflags/buildflags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"  // nogncheck crbug.com/1125897
#endif

namespace {

using extensions::ExtensionIdSet;

const char kExpireDateKey[] = "expire_date";
const char kExpiryKey[] = "expiry";
const char kHashKey[] = "hash";
const char kIdsKey[] = "ids";
const char kInvalidIdsKey[] = "invalid_ids";
const char kProtocolVersionKey[] = "protocol_version";
const char kSaltKey[] = "salt";
const char kSignatureKey[] = "signature";
const char kSignatureFormatVersionKey[] = "signature_format_version";
const char kTimestampKey[] = "timestamp";

const char kContentTypeJSON[] = "application/json";

// This allows us to version the format of what we write into the prefs,
// allowing for forward migration, as well as detecting forwards/backwards
// incompatabilities, etc.
const int kSignatureFormatVersion = 2;

const size_t kSaltBytes = 32;

const char kBackendUrl[] =
    "https://www.googleapis.com/chromewebstore/v1.1/items/verify";

const char kPublicKeyPEM[] =
    "-----BEGIN PUBLIC KEY-----"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAj/u/XDdjlDyw7gHEtaaa"
    "sZ9GdG8WOKAyJzXd8HFrDtz2Jcuy7er7MtWvHgNDA0bwpznbI5YdZeV4UfCEsA4S"
    "rA5b3MnWTHwA1bgbiDM+L9rrqvcadcKuOlTeN48Q0ijmhHlNFbTzvT9W0zw/GKv8"
    "LgXAHggxtmHQ/Z9PP2QNF5O8rUHHSL4AJ6hNcEKSBVSmbbjeVm4gSXDuED5r0nwx"
    "vRtupDxGYp8IZpP5KlExqNu1nbkPc+igCTIB6XsqijagzxewUHCdovmkb2JNtskx"
    "/PMIEv+TvWIx2BzqGp71gSh/dV7SJ3rClvWd2xj8dtxG8FfAWDTIIi0qZXWn2Qhi"
    "zQIDAQAB"
    "-----END PUBLIC KEY-----";

GURL GetBackendUrl() {
  return GURL(kBackendUrl);
}

// Hashes |salt| with the machine id, base64-encodes it and returns it in
// |result|.
bool HashWithMachineId(const std::string& salt, std::string* result) {
  std::string machine_id;
#if BUILDFLAG(ENABLE_RLZ)
  if (!rlz_lib::GetMachineId(&machine_id))
    return false;
#else
  machine_id = "unknown";
#endif

  std::unique_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  hash->Update(base::as_byte_span(machine_id));
  hash->Update(base::as_byte_span(salt));

  std::array<uint8_t, crypto::kSHA256Length> result_bytes;
  hash->Finish(result_bytes);

  *result = base::Base64Encode(result_bytes);
  return true;
}

// Validates that |input| is a string of the form "YYYY-MM-DD".
bool ValidateExpireDateFormat(const std::string& input) {
  if (input.length() != 10)
    return false;
  for (int i = 0; i < 10; i++) {
    if (i == 4 || i == 7) {
      if (input[i] != '-')
        return false;
    } else if (!base::IsAsciiDigit(input[i])) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::optional<ExtensionIdSet> ExtensionIdSetFromList(
    const base::Value::List& list) {
  ExtensionIdSet ids;
  for (const base::Value& value : list) {
    if (!value.is_string())
      return std::nullopt;
    ids.insert(value.GetString());
  }
  return ids;
}

}  // namespace

namespace extensions {

InstallSignature::InstallSignature() = default;
InstallSignature::InstallSignature(const InstallSignature& other) = default;
InstallSignature::~InstallSignature() = default;

base::Value::Dict InstallSignature::ToDict() const {
  base::Value::Dict dict;
  dict.Set(kSignatureFormatVersionKey, kSignatureFormatVersion);
  dict.Set(kIdsKey, base::ToValueList(ids));
  dict.Set(kInvalidIdsKey, base::ToValueList(invalid_ids));
  dict.Set(kExpireDateKey, expire_date);
  dict.Set(kSaltKey, base::Base64Encode(salt));
  dict.Set(kSignatureKey, base::Base64Encode(signature));
  dict.Set(kTimestampKey, base::TimeToValue(timestamp));
  return dict;
}

// static
std::unique_ptr<InstallSignature> InstallSignature::FromDict(
    const base::Value::Dict& dict) {
  std::unique_ptr<InstallSignature> result =
      std::make_unique<InstallSignature>();

  // For now we don't want to support any backwards compability, but in the
  // future if we do, we would want to put the migration code here.
  if (dict.FindInt(kSignatureFormatVersionKey) != kSignatureFormatVersion)
    return nullptr;

  raw_ptr<const std::string> expire_date = dict.FindString(kExpireDateKey);
  raw_ptr<const std::string> salt_base64 = dict.FindString(kSaltKey);
  raw_ptr<const std::string> signature_base64 = dict.FindString(kSignatureKey);
  if (!expire_date || !salt_base64 || !signature_base64 ||
      !base::Base64Decode(*salt_base64, &result->salt) ||
      !base::Base64Decode(*signature_base64, &result->signature))
    return nullptr;

  result->expire_date = *expire_date;

  // Note: earlier versions of the code did not write out a timestamp value
  // so older entries will not necessarily have this.
  result->timestamp =
      base::ValueToTime(dict.Find(kTimestampKey)).value_or(base::Time());

  raw_ptr<const base::Value::List> ids_list = dict.FindList(kIdsKey);
  raw_ptr<const base::Value::List> invalid_ids_list =
      dict.FindList(kInvalidIdsKey);
  if (!ids_list || !invalid_ids_list)
    return nullptr;

  std::optional<ExtensionIdSet> ids = ExtensionIdSetFromList(*ids_list);
  std::optional<ExtensionIdSet> invalid_ids =
      ExtensionIdSetFromList(*invalid_ids_list);
  if (!ids || !invalid_ids)
    return nullptr;

  result->ids = ids.value();
  result->invalid_ids = invalid_ids.value();

  return result;
}

InstallSigner::InstallSigner(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const ExtensionIdSet& ids)
    : ids_(ids), url_loader_factory_(std::move(url_loader_factory)) {}

InstallSigner::~InstallSigner() = default;

// static
bool InstallSigner::VerifySignature(const InstallSignature& signature) {
  if (signature.ids.empty())
    return true;

  std::string signed_data;
  for (auto i = signature.ids.begin(); i != signature.ids.end(); ++i)
    signed_data.append(*i);

  std::string hash_base64;
  if (!HashWithMachineId(signature.salt, &hash_base64))
    return false;
  signed_data.append(hash_base64);

  signed_data.append(signature.expire_date);

  std::string public_key;
  if (!Extension::ParsePEMKeyBytes(kPublicKeyPEM, &public_key))
    return false;

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA1,
                           base::as_bytes(base::make_span(signature.signature)),
                           base::as_bytes(base::make_span(public_key))))
    return false;

  verifier.VerifyUpdate(base::as_bytes(base::make_span(signed_data)));
  return verifier.VerifyFinal();
}

// static
ExtensionIdSet InstallSigner::GetForcedNotFromWebstore() {
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ::switches::kExtensionsNotWebstore);
  if (value.empty())
    return ExtensionIdSet();

  std::vector<std::string> ids = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return ExtensionIdSet(ids.begin(), ids.end());
}

void InstallSigner::GetSignature(SignatureCallback callback) {
  CHECK(!simple_loader_.get());
  CHECK(callback_.is_null());
  CHECK(salt_.empty());
  callback_ = std::move(callback);

  // If the set of ids is empty, just return an empty signature and skip the
  // call to the server.
  if (ids_.empty()) {
    if (!callback_.is_null())
      std::move(callback_).Run(std::make_unique<InstallSignature>());
    return;
  }

  salt_ = std::string(kSaltBytes, 0);
  crypto::RandBytes(base::as_writable_byte_span(salt_));

  std::string hash_base64;
  if (!HashWithMachineId(salt_, &hash_base64)) {
    ReportErrorViaCallback();
    return;
  }

  if (!url_loader_factory_) {
    ReportErrorViaCallback();
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("extension_install_signer", R"(
        semantics {
          sender: "Extension Install Signer"
          description: "Fetches the signatures for installed extensions."
          trigger:
            "Chrome detects an extension that requires installation "
            "verification."
          data:
            "The ids of the extensions that need to be verified, as well as a "
            "non-revertable salted hash of the user's machine id provided by "
            "RLZ library, which varies between different installs. This id is "
            "only used to verify the validity of the response."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled, but it is only activated if "
            "extensions are installed."
          chrome_policy {
            ExtensionInstallBlocklist {
              policy_options {mode: MANDATORY}
              ExtensionInstallBlocklist: {
                entries: '*'
              }
            }
          }
        })");

  // The request protocol is JSON of the form:
  // {
  //   "protocol_version": 1,
  //   "hash": "<base64-encoded hash value here>",
  //   "ids": [ "<id1>", "<id2>" ]
  // }
  base::Value::Dict dictionary;
  dictionary.Set(kProtocolVersionKey, 1);
  dictionary.Set(kHashKey, hash_base64);
  base::Value::List id_list;
  for (const ExtensionId& extension_id : ids_) {
    id_list.Append(extension_id);
  }
  dictionary.Set(kIdsKey, std::move(id_list));
  std::string json;
  base::JSONWriter::Write(dictionary, &json);
  if (json.empty()) {
    ReportErrorViaCallback();
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetBackendUrl();
  resource_request->method = "POST";

  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  simple_loader_->AttachStringForUpload(json, kContentTypeJSON);

  request_start_time_ = base::Time::Now();
  VLOG(1) << "Sending request: " << json;

  // TODO: Set a cap value to the expected content to be loaded, and use
  // DownloadToString instead.
  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&InstallSigner::ParseFetchResponse,
                     base::Unretained(this)));
}

void InstallSigner::ReportErrorViaCallback() {
  if (!callback_.is_null())
    std::move(callback_).Run(nullptr);
}

void InstallSigner::ParseFetchResponse(
    std::unique_ptr<std::string> response_body) {
  if (!response_body || response_body->empty()) {
    ReportErrorViaCallback();
    return;
  }
  VLOG(1) << "Got response: " << *response_body;

  // The response is JSON of the form:
  // {
  //   "protocol_version": 1,
  //   "signature": "<base64-encoded signature>",
  //   "expiry": "<date in YYYY-MM-DD form>",
  //   "invalid_ids": [ "<id3>", "<id4>" ]
  // }
  // where |invalid_ids| is a list of ids from the original request that
  // could not be verified to be in the webstore.

  std::optional<base::Value> parsed = base::JSONReader::Read(*response_body);
  bool json_success = parsed && parsed->is_dict();
  if (!json_success) {
    ReportErrorViaCallback();
    return;
  }
  base::Value::Dict& dictionary = parsed->GetDict();

  int protocol_version = dictionary.FindInt(kProtocolVersionKey).value_or(0);
  std::string signature_base64;
  std::string signature;
  std::string expire_date;

  if (const std::string* maybe_signature_base64 =
          dictionary.FindString(kSignatureKey)) {
    signature_base64 = *maybe_signature_base64;
  }
  if (const std::string* maybe_expire_date =
          dictionary.FindString(kExpiryKey)) {
    expire_date = *maybe_expire_date;
  }

  bool fields_success = protocol_version == 1 && !signature_base64.empty() &&
                        ValidateExpireDateFormat(expire_date) &&
                        base::Base64Decode(signature_base64, &signature);
  if (!fields_success) {
    ReportErrorViaCallback();
    return;
  }

  ExtensionIdSet invalid_ids;
  const base::Value::List* invalid_ids_list =
      dictionary.FindList(kInvalidIdsKey);
  if (invalid_ids_list) {
    for (const base::Value& invalid_id : *invalid_ids_list) {
      const std::string* id = invalid_id.GetIfString();
      if (!id) {
        ReportErrorViaCallback();
        return;
      }
      invalid_ids.insert(*id);
    }
  }

  HandleSignatureResult(signature, expire_date, invalid_ids);
}

void InstallSigner::HandleSignatureResult(const std::string& signature,
                                          const std::string& expire_date,
                                          const ExtensionIdSet& invalid_ids) {
  ExtensionIdSet valid_ids =
      base::STLSetDifference<ExtensionIdSet>(ids_, invalid_ids);

  std::unique_ptr<InstallSignature> result;
  if (!signature.empty()) {
    result = std::make_unique<InstallSignature>();
    result->ids = valid_ids;
    result->invalid_ids = invalid_ids;
    result->salt = salt_;
    result->signature = signature;
    result->expire_date = expire_date;
    result->timestamp = request_start_time_;
    if (!VerifySignature(*result))
      result.reset();
  }

  if (!callback_.is_null())
    std::move(callback_).Run(std::move(result));
}

}  // namespace extensions
