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
    case manta::MantaStatusCode::kResourceExhausted:
      return MojomMantaStatusCode::kResourceExhausted;
    case manta::MantaStatusCode::kNoInternetConnection:
      return MojomMantaStatusCode::kNoInternetConnection;
    case manta::MantaStatusCode::kUnsupportedLanguage:
      return MojomMantaStatusCode::kUnsupportedLanguage;
    case manta::MantaStatusCode::kBlockedOutputs:
      return MojomMantaStatusCode::kBlockedOutputs;
    case manta::MantaStatusCode::kPerUserQuotaExceeded:
      return MojomMantaStatusCode::kPerUserQuotaExceeded;
    case manta::MantaStatusCode::kGenericError:
    case manta::MantaStatusCode::kInvalidInput:
    case manta::MantaStatusCode::kBackendFailure:
    case manta::MantaStatusCode::kMalformedResponse:
    case manta::MantaStatusCode::kRestrictedCountry:
    case manta::MantaStatusCode::kNoIdentityManager:
      return MojomMantaStatusCode::kGenericError;
    default:
      NOTREACHED();
  }
}

bool EnumTraits<MojomMantaStatusCode, manta::MantaStatusCode>::FromMojom(
    MojomMantaStatusCode input,
    manta::MantaStatusCode* output) {
  switch (input) {
    case MojomMantaStatusCode::kOk:
      *output = manta::MantaStatusCode::kOk;
      return true;
    case MojomMantaStatusCode::kResourceExhausted:
      *output = manta::MantaStatusCode::kResourceExhausted;
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
    case MojomMantaStatusCode::kPerUserQuotaExceeded:
      *output = manta::MantaStatusCode::kPerUserQuotaExceeded;
      return true;
    case MojomMantaStatusCode::kGenericError:
    case MojomMantaStatusCode::kInvalidInput:
    case MojomMantaStatusCode::kBackendFailure:
    case MojomMantaStatusCode::kMalformedResponse:
    case MojomMantaStatusCode::kRestrictedCountry:
    case MojomMantaStatusCode::kNoIdentityManager:
      *output = manta::MantaStatusCode::kGenericError;
      return true;
  }
  NOTREACHED();
}
}  // namespace mojo
