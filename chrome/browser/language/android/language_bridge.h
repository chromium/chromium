// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_ANDROID_LANGUAGE_BRIDGE_H_
#define CHROME_BROWSER_LANGUAGE_ANDROID_LANGUAGE_BRIDGE_H_

#include <string>
#include <vector>

namespace language {
class LanguageBridge {
 public:
  // Makes a blocking call to get ULP languages for |account_name| from device.
  static std::vector<std::string> GetULPLanguagesFromDevice(
      std::string account_name);
};

}  // namespace language

#endif  // CHROME_BROWSER_LANGUAGE_ANDROID_LANGUAGE_BRIDGE_H_
