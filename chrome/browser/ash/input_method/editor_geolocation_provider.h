// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_PROVIDER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_PROVIDER_H_

#include <string>

namespace ash::input_method {

class EditorGeolocationProvider {
 public:
  virtual ~EditorGeolocationProvider() = default;

  virtual std::string GetCountryCode() = 0;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_PROVIDER_H_
