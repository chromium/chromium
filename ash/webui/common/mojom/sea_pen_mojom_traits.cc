// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/mojom/sea_pen_mojom_traits.h"

#include "ash/webui/common/mojom/sea_pen.mojom-shared.h"
#include "components/manta/manta_status.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

using MojomMantaStatusCode = ash::personalization_app::mojom::MantaStatusCode;

MojomMantaStatusCode
EnumTraits<MojomMantaStatusCode, manta::MantaStatusCode>::ToMojom(
    manta::MantaStatusCode input) {
  switch (input) {
    case manta::MantaStatusCode::kOk:
      return MojomMantaStatusCode::kOk;
    case manta::MantaStatusCode::kGenericError:
      return MojomMantaStatusCode::kGenericError;
    case manta::MantaStatusCode::kInvalidInput:
      return MojomMantaStatusCode::kInvalidInput;
    case manta::MantaStatusCode::kResourceExhausted:
      return MojomMantaStatusCode::kResourceExhausted;
    case manta::MantaStatusCode::kBackendFailure:
      return MojomMantaStatusCode::kBackendFailure;
    case manta::MantaStatusCode::kMalformedResponse:
      return MojomMantaStatusCode::kMalformedResponse;
    case manta::MantaStatusCode::kNoInternetConnection:
      return MojomMantaStatusCode::kNoInternetConnection;
    case manta::MantaStatusCode::kUnsupportedLanguage:
      return MojomMantaStatusCode::kUnsupportedLanguage;
    case manta::MantaStatusCode::kRestrictedCountry:
      return MojomMantaStatusCode::kRestrictedCountry;
    case manta::MantaStatusCode::kNoIdentityManager:
      return MojomMantaStatusCode::kNoIdentityManager;
    case manta::MantaStatusCode::kPerUserQuotaExceeded:
      return MojomMantaStatusCode::kPerUserQuotaExceeded;
    default:
      NOTREACHED();
      return MojomMantaStatusCode::kGenericError;
  }
}

bool EnumTraits<MojomMantaStatusCode, manta::MantaStatusCode>::FromMojom(
    MojomMantaStatusCode input,
    manta::MantaStatusCode* output) {
  switch (input) {
    case MojomMantaStatusCode::kOk:
      *output = manta::MantaStatusCode::kOk;
      return true;
    case MojomMantaStatusCode::kGenericError:
      *output = manta::MantaStatusCode::kGenericError;
      return true;
    case MojomMantaStatusCode::kInvalidInput:
      *output = manta::MantaStatusCode::kInvalidInput;
      return true;
    case MojomMantaStatusCode::kResourceExhausted:
      *output = manta::MantaStatusCode::kResourceExhausted;
      return true;
    case MojomMantaStatusCode::kBackendFailure:
      *output = manta::MantaStatusCode::kBackendFailure;
      return true;
    case MojomMantaStatusCode::kMalformedResponse:
      *output = manta::MantaStatusCode::kMalformedResponse;
      return true;
    case MojomMantaStatusCode::kNoInternetConnection:
      *output = manta::MantaStatusCode::kNoInternetConnection;
      return true;
    case MojomMantaStatusCode::kUnsupportedLanguage:
      *output = manta::MantaStatusCode::kUnsupportedLanguage;
      return true;
    case MojomMantaStatusCode::kBlockedOutputs:
      *output = manta::MantaStatusCode::kBlockedOutputs;
      return true;
    case MojomMantaStatusCode::kRestrictedCountry:
      *output = manta::MantaStatusCode::kRestrictedCountry;
      return true;
    case MojomMantaStatusCode::kNoIdentityManager:
      *output = manta::MantaStatusCode::kNoIdentityManager;
      return true;
    case MojomMantaStatusCode::kPerUserQuotaExceeded:
      *output = manta::MantaStatusCode::kPerUserQuotaExceeded;
      return true;
  }
  NOTREACHED();
  return false;
}
}  // namespace mojo
