// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_geolocation_mock_provider.h"

namespace ash::input_method {

EditorGeolocationMockProvider::EditorGeolocationMockProvider(
    std::string_view country_code)
    : country_code_(country_code) {}

std::string EditorGeolocationMockProvider::GetCountryCode() {
  return country_code_;
}

}  // namespace ash::input_method
