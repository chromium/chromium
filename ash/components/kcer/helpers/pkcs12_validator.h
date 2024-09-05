// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains a group of functions which are used for pkcs12 data
// validation before data import to chaps. They are used in kcer_chaps_util.cc,
// but they are not related to chaps, so they were moved to a separate file.

#ifndef ASH_COMPONENTS_KCER_HELPERS_PKCS12_VALIDATOR_H_
#define ASH_COMPONENTS_KCER_HELPERS_PKCS12_VALIDATOR_H_

#include <string>
#include <vector>

#include "ash/components/kcer/cert_cache.h"
#include "ash/components/kcer/helpers/pkcs12_reader.h"
#include "base/memory/raw_ref.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/stack.h"

namespace kcer::internal {

// Returns an error message corresponding to the given import error code.
std::string MakePkcs12CertImportErrorMessage(Pkcs12ReaderStatusCode error_code);

Error ConvertPkcs12ParsingError(Pkcs12ReaderStatusCode status);

// Finds or creates the correct nickname for the `cert` taking the
// `existing_certs` into account.
Pkcs12ReaderStatusCode GetNickname(
    const std::vector<scoped_refptr<const Cert>>& existing_certs,
    const base::flat_set<std::string_view>& existing_nicknames,
    X509* cert,
    const Pkcs12Reader& pkcs12_reader,
    std::string& cert_nickname);

// Filter out certs from (`certs`) which are not directly related to key_data
// (`key_data`), extracts nickname from the certificate or placing default
// nickname and stores certificates which will be installed to
// (`valid_certs_data`). Exported for unit tests only.
COMPONENT_EXPORT(KCER)
Pkcs12ReaderStatusCode ValidateAndPrepareCertData(
    const CertCache& cert_cache,
    const Pkcs12Reader& pkcs12_reader,
    bssl::UniquePtr<STACK_OF(X509)> certs,
    KeyData& key_data,
    std::vector<CertData>& valid_certs_data);

}  // namespace kcer::internal

#endif  // ASH_COMPONENTS_KCER_HELPERS_PKCS12_VALIDATOR_H_
