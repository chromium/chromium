// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/limited_access_features.h"

#include <inspectable.h>
#include <roapi.h>
#include <windows.foundation.h>
#include <windows.services.store.h>
#include <wrl.h>

#include <string>

#include "base/strings/strcat.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

using ABI::Windows::ApplicationModel::IID_ILimitedAccessFeaturesStatics;
using ABI::Windows::ApplicationModel::ILimitedAccessFeatureRequestResult;
using ABI::Windows::ApplicationModel::ILimitedAccessFeaturesStatics;
using ABI::Windows::ApplicationModel::LimitedAccessFeatureStatus;
using ABI::Windows::ApplicationModel::LimitedAccessFeatureStatus_Available;
using ABI::Windows::ApplicationModel::
    LimitedAccessFeatureStatus_AvailableWithoutToken;
using Microsoft::WRL::ComPtr;

namespace {

// Microsoft provided these values. They are used to unlock and access limited
// access features.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr wchar_t kLimitedAccessFeatureIdentity[] = L"0qgpfzgh1edfy";
#else
constexpr wchar_t kLimitedAccessFeatureIdentity[] = L"b06a12530me7r";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

namespace base::win {

bool TryToUnlockLimitedAccessFeature(const std::wstring& feature,
                                     const std::wstring& token) {
  ComPtr<ILimitedAccessFeaturesStatics> limited_access_features;
  ComPtr<ILimitedAccessFeatureRequestResult> limited_access_features_result;

  HRESULT hr = base::win::RoGetActivationFactory(
      HStringReference(
          RuntimeClass_Windows_ApplicationModel_LimitedAccessFeatures)
          .Get(),
      IID_ILimitedAccessFeaturesStatics, &limited_access_features);

  if (!SUCCEEDED(hr)) {
    return false;
  }

  // Required to unlock feature.
  const std::wstring attestation =
      StrCat({kLimitedAccessFeatureIdentity, L" has registered their use of ",
              feature, L" with Microsoft and agrees to the terms of use."});

  hr = limited_access_features->TryUnlockFeature(
      HStringReference(feature.c_str()).Get(),
      HStringReference(token.c_str()).Get(),
      HStringReference(attestation.c_str()).Get(),
      &limited_access_features_result);
  if (!SUCCEEDED(hr)) {
    return false;
  }

  LimitedAccessFeatureStatus status;
  hr = limited_access_features_result->get_Status(&status);
  if (!SUCCEEDED(hr)) {
    return false;
  }

  if ((status != LimitedAccessFeatureStatus_Available) &&
      (status != LimitedAccessFeatureStatus_AvailableWithoutToken)) {
    return false;
  }
  return true;
}

}  // namespace base::win
