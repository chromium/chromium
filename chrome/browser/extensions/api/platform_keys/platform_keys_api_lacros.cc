// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_api_lacros.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/platform_keys_internal.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace api_pki = api::platform_keys_internal;
using SigningScheme = crosapi::mojom::KeystoreSigningScheme;
using SigningAlgorithmName = crosapi::mojom::KeystoreSigningAlgorithmName;
using KeystoreService = crosapi::mojom::KeystoreService;

//------------------------------------------------------------------------------

PlatformKeysVerifyTLSServerCertificateFunction::
    ~PlatformKeysVerifyTLSServerCertificateFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysVerifyTLSServerCertificateFunction::Run() {
  return RespondNow(Error("Not implemented."));
}

}  // namespace extensions
