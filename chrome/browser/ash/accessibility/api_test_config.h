// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_API_TEST_CONFIG_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_API_TEST_CONFIG_H_

#include "chrome/browser/extensions/extension_browsertest.h"

namespace ash {

using ContextType = ::extensions::ExtensionBrowserTest::ContextType;

enum class ManifestVersion { kTwo, kThree };

// A class used to define the parameters of an API test case.
class ApiTestConfig {
 public:
  ApiTestConfig(ContextType context_type, ManifestVersion version)
      : context_type_(context_type), version_(version) {}

  ContextType context_type() const { return context_type_; }
  ManifestVersion version() const { return version_; }

 private:
  ContextType context_type_;
  ManifestVersion version_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_API_TEST_CONFIG_H_
