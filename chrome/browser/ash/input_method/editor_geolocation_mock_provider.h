// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_MOCK_PROVIDER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_MOCK_PROVIDER_H_

#include <string>

#include "chrome/browser/ash/input_method/editor_geolocation_provider.h"

namespace ash::input_method {

class EditorGeolocationMockProvider : public EditorGeolocationProvider {
 public:
  explicit EditorGeolocationMockProvider(std::string_view country_code);
  ~EditorGeolocationMockProvider() override = default;

  std::string GetCountryCode() override;

 private:
  std::string country_code_;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_MOCK_PROVIDER_H_
