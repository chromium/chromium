// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_geolocation_provider_from_finch.h"

#include "base/check.h"
#include "components/variations/service/variations_service.h"

namespace ash::input_method {

EditorGeolocationProviderFromFinch::EditorGeolocationProviderFromFinch(
    variations::VariationsService* variations_service)
    : variations_service_(variations_service) {
  CHECK(variations_service_);
  observation_.Observe(variations_service_);
}

EditorGeolocationProviderFromFinch::~EditorGeolocationProviderFromFinch() =
    default;

std::string EditorGeolocationProviderFromFinch::GetCountryCode() {
  CHECK(variations_service_);
  return variations_service_->GetLatestCountry();
}

void EditorGeolocationProviderFromFinch::OnVariationsServiceDestroyed() {
  // `VariationsService` is destroyed before `KeyedService`s during
  // `BrowserProcessImpl::StartTearDown()`.
  observation_.Reset();
  variations_service_ = nullptr;
}

}  // namespace ash::input_method
