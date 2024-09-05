// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/kcer/helpers/pkcs12_validator.h"

#include <cert.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/components/kcer/cert_cache.h"
#include "ash/components/kcer/helpers/pkcs12_reader.h"
#include "ash/components/kcer/kcer.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pkcs8.h"
#include "third_party/boringssl/src/include/openssl/stack.h"

namespace kcer::internal {
namespace {

constexpr char kPkcs12CertImportFailed[] =
    "Chaps util cert import failed with ";
constexpr int kMaxAttemptUniqueNicknameCreation = 100;
constexpr const char kDefaultNickname[] = "Unknown org";

// Custom CERTCertificateList object allows to avoid calls to PORT_FreeArena()
// after every usage of CERTCertificateList.
struct CERTCertificateListDeleter {
  void operator()(CERTCertificateList* cert_list) {
    CERT_DestroyCertificateList(cert_list);
  }
};
using Pkcs12ScopedCERTCertificateList =
    std::unique_ptr<CERTCertificateList, CERTCertificateListDeleter>;

std::string AddUniqueIndex(std::string old_name, int unique_number) {
  if (unique_number == 0) {
    return old_name;
  }
  return old_name + " " + base::NumberToString(unique_number);
}

Pkcs12ReaderStatusCode MakeNicknameUnique(
    const base::flat_set<std::string_view>& existing_nicknames,
    const std::string& nickname,
    const Pkcs12Reader& pkcs12_reader,
    std::string& unique_nickname) {
  int unique_counter = 0;
  std::string temp_nickname;
  while (unique_counter < kMaxAttemptUniqueNicknameCreation) {
    temp_nickname = AddUniqueIndex(nickname, unique_counter);
    if (!base::Contains(existing_nicknames, temp_nickname)) {
      unique_nickname = temp_nickname;
      return Pkcs12ReaderStatusCode::kSuccess;
    }
    unique_counter++;
  }

  return Pkcs12ReaderStatusCode::kPkcs12ReachedMaxAttemptForUniqueness;
}

Pkcs12ReaderStatusCode GetFirstCertNicknameWithSubject(
    const std::vector<scoped_refptr<const Cert>>& existing_certs,
    const Pkcs12Reader& pkcs12_reader,
    base::span<const uint8_t> required_subject_name,
    std::string& previously_used_nickname) {
  for (const scoped_refptr<const Cert>& cert : existing_certs) {
    ScopedX509 x509(
        X509_parse_from_buffer(cert.get()->GetX509Cert()->cert_buffer()));
    if (!x509) {
      continue;
    }

    base::span<const uint8_t> current_subject_name;
    if (pkcs12_reader.GetSubjectNameDer(x509.get(), current_subject_name) !=
        Pkcs12ReaderStatusCode::kSuccess) {
      continue;
    }

    if (!base::ranges::equal(required_subject_name, current_subject_name)) {
      continue;
    }

    if (cert.get()->GetNickname().empty()) {
      continue;
    }

    previously_used_nickname = cert.get()->GetNickname();
    return Pkcs12ReaderStatusCode::kSuccess;
  }

  return Pkcs12ReaderStatusCode::kPkcs12NoNicknamesWasExtracted;
}

}  // namespace

std::string MakePkcs12CertImportErrorMessage(
    Pkcs12ReaderStatusCode error_code) {
  return kPkcs12CertImportFailed +
         base::NumberToString(static_cast<int>(error_code));
}

Error ConvertPkcs12ParsingError(Pkcs12ReaderStatusCode status) {
  switch (status) {
    case Pkcs12ReaderStatusCode::kPkcs12WrongPassword:
      return Error::kPkcs12WrongPassword;
    case Pkcs12ReaderStatusCode::kPkcs12InvalidMac:
      return Error::kPkcs12InvalidMac;
    case Pkcs12ReaderStatusCode::kPkcs12InvalidFile:
      return Error::kInvalidPkcs12;
    case Pkcs12ReaderStatusCode::kPkcs12UnsupportedFile:
    default:
      return Error::kFailedToParsePkcs12;
  }
}

Pkcs12ReaderStatusCode GetNickname(
    const std::vector<scoped_refptr<const Cert>>& existing_certs,
    const base::flat_set<std::string_view>& existing_nicknames,
    X509* cert,
    const Pkcs12Reader& pkcs12_reader,
    std::string& cert_nickname) {
  base::span<const uint8_t> required_subject;
  Pkcs12ReaderStatusCode get_subject_name_result =
      pkcs12_reader.GetSubjectNameDer(cert, required_subject);
  if (get_subject_name_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_subject_name_result);
    return get_subject_name_result;
  }

  std::string already_used_nickname;
  Pkcs12ReaderStatusCode fetch_certs_result = GetFirstCertNicknameWithSubject(
      existing_certs, pkcs12_reader, required_subject, already_used_nickname);

  bool acceptable_fetch_certs_result =
      fetch_certs_result == Pkcs12ReaderStatusCode::kSuccess ||
      fetch_certs_result ==
          Pkcs12ReaderStatusCode::kPkcs12NoNicknamesWasExtracted;

  if (!acceptable_fetch_certs_result) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(fetch_certs_result);
    return fetch_certs_result;
  }

  if (fetch_certs_result == Pkcs12ReaderStatusCode::kSuccess &&
      !already_used_nickname.empty()) {
    cert_nickname = already_used_nickname;
    return Pkcs12ReaderStatusCode::kSuccess;
  }

  // No certs with the same subject were found, will try to extract nickname
  // from the cert.
  std::string nickname;
  Pkcs12ReaderStatusCode nickname_extraction_result =
      pkcs12_reader.GetLabel(cert, nickname);
  if (nickname_extraction_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(WARNING) << MakePkcs12CertImportErrorMessage(
        nickname_extraction_result);
  }

  if (nickname.empty()) {
    // We did try our best, giving default nickname.
    nickname = kDefaultNickname;
  }

  std::string new_unique_nickname;
  Pkcs12ReaderStatusCode make_nickname_uniq_result = MakeNicknameUnique(
      existing_nicknames, nickname, pkcs12_reader, new_unique_nickname);
  if (make_nickname_uniq_result != Pkcs12ReaderStatusCode::kSuccess) {
    LOG(ERROR) << MakePkcs12CertImportErrorMessage(make_nickname_uniq_result);
    return make_nickname_uniq_result;
  }

  cert_nickname = new_unique_nickname;
  return Pkcs12ReaderStatusCode::kSuccess;
}

Pkcs12ReaderStatusCode ValidateAndPrepareCertData(
    const CertCache& cert_cache,
    const Pkcs12Reader& pkcs12_reader,
    bssl::UniquePtr<STACK_OF(X509)> certs,
    KeyData& key_data,
    std::vector<CertData>& valid_certs_data) {
  if (!certs) {
    return Pkcs12ReaderStatusCode::kCertificateDataMissed;
  }
  if (!key_data.key) {
    return Pkcs12ReaderStatusCode::kKeyDataMissed;
  }

  std::vector<scoped_refptr<const Cert>> existing_certs =
      cert_cache.GetAllCerts();

  std::vector<std::string_view> existing_nicknames;
  existing_nicknames.reserve(existing_certs.size());
  for (const auto& existing_cert : existing_certs) {
    existing_nicknames.push_back(
        std::string_view(existing_cert->GetNickname()));
  }

  int already_imported_cert_counter = 0;

  // Normal case if there is one private key and one certificate in pkcs12, but
  // it might be the whole chain included. All certs that are not directly
  // related to the key will be filtered out.
  std::string cert_nickname;
  while (sk_X509_num(certs.get()) > 0) {
    bssl::UniquePtr<X509> cert(sk_X509_pop(certs.get()));
    if (!cert) {
      LOG(WARNING) << MakePkcs12CertImportErrorMessage(
          Pkcs12ReaderStatusCode::kCertificateDataMissed);
      continue;
    }

    int cert_der_size = 0;
    bssl::UniquePtr<uint8_t> cert_der;
    Pkcs12ReaderStatusCode get_cert_der_result =
        pkcs12_reader.GetDerEncodedCert(cert.get(), cert_der, cert_der_size);
    if (get_cert_der_result != Pkcs12ReaderStatusCode::kSuccess) {
      LOG(ERROR) << MakePkcs12CertImportErrorMessage(get_cert_der_result);
      return get_cert_der_result;
    }
    CertDer cert_der_typed(
        std::vector<uint8_t>(cert_der.get(), cert_der.get() + cert_der_size));

    if (cert_cache.FindCert(cert_der_typed.value())) {
      LOG(WARNING) << "Cert is already installed, skipping";
      ++already_imported_cert_counter;
      continue;
    }

    bool is_cert_related_to_key = false;
    Pkcs12ReaderStatusCode cert_to_key_check_result =
        pkcs12_reader.CheckRelation(key_data, cert.get(),
                                    is_cert_related_to_key);
    if (cert_to_key_check_result != Pkcs12ReaderStatusCode::kSuccess) {
      LOG(ERROR) << MakePkcs12CertImportErrorMessage(cert_to_key_check_result);
      continue;
    }
    if (!is_cert_related_to_key) {
      LOG(WARNING) << "Cert is not directly related to key, skipping";
      continue;
    }

    if (cert_nickname.empty()) {
      Pkcs12ReaderStatusCode get_cert_nickname_result =
          GetNickname(existing_certs, existing_nicknames, cert.get(),
                      pkcs12_reader, cert_nickname);
      if (get_cert_nickname_result != Pkcs12ReaderStatusCode::kSuccess) {
        LOG(WARNING) << "Can not get nickname for the certificate due to: "
                     << MakePkcs12CertImportErrorMessage(
                            get_cert_nickname_result);
        continue;
      }
    }

    CertData& cert_data = valid_certs_data.emplace_back();
    cert_data.x509 = std::move(cert);
    cert_data.nickname = std::move(cert_nickname);
    cert_data.cert_der = std::move(cert_der_typed);
  }

  if (valid_certs_data.size() > 0) {
    return Pkcs12ReaderStatusCode::kSuccess;
  }
  if (already_imported_cert_counter > 0) {
    return Pkcs12ReaderStatusCode::kAlreadyExists;
  }

  return Pkcs12ReaderStatusCode::kPkcs12NoValidCertificatesFound;
}

}  // namespace kcer::internal
