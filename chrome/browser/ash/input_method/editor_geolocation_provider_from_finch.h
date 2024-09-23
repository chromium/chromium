// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_PROVIDER_FROM_FINCH_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_PROVIDER_FROM_FINCH_H_

#include <string>

#include "chrome/browser/ash/input_method/editor_geolocation_provider.h"

namespace ash::input_method {

class EditorGeolocationProviderFromFinch : public EditorGeolocationProvider {
 public:
  EditorGeolocationProviderFromFinch() = default;
  ~EditorGeolocationProviderFromFinch() override = default;

  std::string GetCountryCode() override;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_PROVIDER_FROM_FINCH_H_
