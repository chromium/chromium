// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_signer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
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

const char kPublicKeyPEM[] =                                            \
    "-----BEGIN PUBLIC KEY-----"                                        \
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAj/u/XDdjlDyw7gHEtaaa"  \
    "sZ9GdG8WOKAyJzXd8HFrDtz2Jcuy7er7MtWvHgNDA0bwpznbI5YdZeV4UfCEsA4S"  \
    "rA5b3MnWTHwA1bgbiDM+L9rrqvcadcKuOlTeN48Q0ijmhHlNFbTzvT9W0zw/GKv8"  \
    "LgXAHggxtmHQ/Z9PP2QNF5O8rUHHSL4AJ6hNcEKSBVSmbbjeVm4gSXDuED5r0nwx"  \
    "vRtupDxGYp8IZpP5KlExqNu1nbkPc+igCTIB6XsqijagzxewUHCdovmkb2JNtskx"  \
    "/PMIEv+TvWIx2BzqGp71gSh/dV7SJ3rClvWd2xj8dtxG8FfAWDTIIi0qZXWn2Qhi"  \
    "zQIDAQAB"                                                          \
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

  hash->Update(machine_id.data(), machine_id.size());
  hash->Update(salt.data(), salt.size());

  std::string result_bytes(crypto::kSHA256Length, 0);
  hash->Finish(base::data(result_bytes), result_bytes.size());

  base::Base64Encode(result_bytes, result);
  return true;
}

// Validates that |input| is a string of the form "YYYY-MM-DD".
bool ValidateExpireDateFormat(const std::string& input) {
  if (input.length() != 10)
    return false;
  for (int i = 0; i < 10; i++) {
    if (i == 4 ||  i == 7) {
      if (input[i] != '-')
        return false;
    } else if (!base::IsAsciiDigit(input[i])) {
      return false;
    }
  }
  return true;
}

// Sets the value of |key| in |dictionary| to be a list with the contents of
// |ids|.
void SetExtensionIdSet(base::DictionaryValue* dictionary,
                       const char* key,
                       const ExtensionIdSet& ids) {
  auto id_list = std::make_unique<base::ListValue>();
  for (auto i = ids.begin(); i != ids.end(); ++i)
    id_list->AppendString(*i);
  dictionary->Set(key, std::move(id_list));
}

// Tries to fetch a list of strings from |dictionay| for |key|, and inserts
// them into |ids|. The return value indicates success/failure. Note: on
// failure, |ids| might contain partial results, for instance if some of the
// members of the list were not strings.
bool GetExtensionIdSet(const base::DictionaryValue& dictionary,
                       const char* key,
                       ExtensionIdSet* ids) {
  const base::ListValue* id_list = nullptr;
  if (!dictionary.GetList(key, &id_list))
    return false;
  for (const auto& entry : id_list->GetList()) {
    if (!entry.is_string()) {
      return false;
    }
    ids->insert(entry.GetString());
  }
  return true;
}

}  // namespace

namespace extensions {

InstallSignature::InstallSignature() {
}
InstallSignature::InstallSignature(const InstallSignature& other) = default;
InstallSignature::~InstallSignature() {
}

void InstallSignature::ToValue(base::DictionaryValue* value) const {
  CHECK(value);

  value->SetInteger(kSignatureFormatVersionKey, kSignatureFormatVersion);
  SetExtensionIdSet(value, kIdsKey, ids);
  SetExtensionIdSet(value, kInvalidIdsKey, invalid_ids);
  value->SetString(kExpireDateKey, expire_date);
  std::string salt_base64;
  std::string signature_base64;
  base::Base64Encode(salt, &salt_base64);
  base::Base64Encode(signature, &signature_base64);
  value->SetString(kSaltKey, salt_base64);
  value->SetString(kSignatureKey, signature_base64);
  value->SetString(kTimestampKey,
                   base::NumberToString(timestamp.ToInternalValue()));
}

// static
std::unique_ptr<InstallSignature> InstallSignature::FromValue(
    const base::DictionaryValue& value) {
  std::unique_ptr<InstallSignature> result(new InstallSignature);

  // For now we don't want to support any backwards compability, but in the
  // future if we do, we would want to put the migration code here.
  int format_version = 0;
  if (!value.GetInteger(kSignatureFormatVersionKey, &format_version) ||
      format_version != kSignatureFormatVersion) {
    result.reset();
    return result;
  }

  std::string salt_base64;
  std::string signature_base64;
  if (!value.GetString(kExpireDateKey, &result->expire_date) ||
      !value.GetString(kSaltKey, &salt_base64) ||
      !value.GetString(kSignatureKey, &signature_base64) ||
      !base::Base64Decode(salt_base64, &result->salt) ||
      !base::Base64Decode(signature_base64, &result->signature)) {
    result.reset();
    return result;
  }

  // Note: earlier versions of the code did not write out a timestamp value
  // so older entries will not necessarily have this.
  if (value.HasKey(kTimestampKey)) {
    std::string timestamp;
    int64_t timestamp_value = 0;
    if (!value.GetString(kTimestampKey, &timestamp) ||
        !base::StringToInt64(timestamp, &timestamp_value)) {
      result.reset();
      return result;
    }
    result->timestamp = base::Time::FromInternalValue(timestamp_value);
  }

  if (!GetExtensionIdSet(value, kIdsKey, &result->ids) ||
      !GetExtensionIdSet(value, kInvalidIdsKey, &result->invalid_ids)) {
    result.reset();
    return result;
  }

  return result;
}

InstallSigner::InstallSigner(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const ExtensionIdSet& ids)
    : ids_(ids), url_loader_factory_(std::move(url_loader_factory)) {}

InstallSigner::~InstallSigner() {
}

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
  crypto::RandBytes(base::data(salt_), salt_.size());

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
  //   "protocol_version": "1",
  //   "hash": "<base64-encoded hash value here>",
  //   "ids": [ "<id1>", "id2" ]
  // }
  base::DictionaryValue dictionary;
  dictionary.SetInteger(kProtocolVersionKey, 1);
  dictionary.SetString(kHashKey, hash_base64);
  std::unique_ptr<base::ListValue> id_list(new base::ListValue);
  for (auto i = ids_.begin(); i != ids_.end(); ++i) {
    id_list->AppendString(*i);
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
  //   "protocol_version": "1",
  //   "signature": "<base64-encoded signature>",
  //   "expiry": "<date in YYYY-MM-DD form>",
  //   "invalid_ids": [ "<id3>", "<id4>" ]
  // }
  // where |invalid_ids| is a list of ids from the original request that
  // could not be verified to be in the webstore.

  base::DictionaryValue* dictionary = NULL;
  std::unique_ptr<base::Value> parsed =
      base::JSONReader::ReadDeprecated(*response_body);
  bool json_success = parsed.get() && parsed->GetAsDictionary(&dictionary);
  if (!json_success) {
    ReportErrorViaCallback();
    return;
  }

  int protocol_version = 0;
  std::string signature_base64;
  std::string signature;
  std::string expire_date;

  dictionary->GetInteger(kProtocolVersionKey, &protocol_version);
  dictionary->GetString(kSignatureKey, &signature_base64);
  dictionary->GetString(kExpiryKey, &expire_date);

  bool fields_success =
      protocol_version == 1 && !signature_base64.empty() &&
      ValidateExpireDateFormat(expire_date) &&
      base::Base64Decode(signature_base64, &signature);
  if (!fields_success) {
    ReportErrorViaCallback();
    return;
  }

  ExtensionIdSet invalid_ids;
  const base::ListValue* invalid_ids_list = NULL;
  if (dictionary->GetList(kInvalidIdsKey, &invalid_ids_list)) {
    for (size_t i = 0; i < invalid_ids_list->GetSize(); i++) {
      std::string id;
      if (!invalid_ids_list->GetString(i, &id)) {
        ReportErrorViaCallback();
        return;
      }
      invalid_ids.insert(id);
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
