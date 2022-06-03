// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_MOCK_ALTERNATIVE_BROWSER_DRIVER_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_MOCK_ALTERNATIVE_BROWSER_DRIVER_H_

#include <string>

#include "chrome/browser/browser_switcher/alternative_browser_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace browser_switcher {

class MockAlternativeBrowserDriver : public AlternativeBrowserDriver {
 public:
  MockAlternativeBrowserDriver();
  ~MockAlternativeBrowserDriver() override;

  MOCK_CONST_METHOD1(ExpandEnvVars, void(std::string*));
  MOCK_CONST_METHOD1(ExpandPresetBrowsers, void(std::string*));
  MOCK_METHOD2(TryLaunch, void(const GURL&, LaunchCallback cb));
  MOCK_CONST_METHOD0(GetBrowserName, std::string());
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_MOCK_ALTERNATIVE_BROWSER_DRIVER_H_
