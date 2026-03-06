// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTISTEP_FILTER_TEST_UTILS_FAKE_TAB_INTERFACE_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_TEST_UTILS_FAKE_TAB_INTERFACE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;

namespace multistep_filter {

class FakeTabInterface : public tabs::MockTabInterface {
 public:
  explicit FakeTabInterface(Profile* profile,
                            content::WebContents* web_contents);
  ~FakeTabInterface() override;

  content::WebContents* GetContents() const override;
  BrowserWindowInterface* GetBrowserWindowInterface() override;
  const BrowserWindowInterface* GetBrowserWindowInterface() const override;

 private:
  raw_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
};

}  // namespace multistep_filter

#endif  // CHROME_BROWSER_MULTISTEP_FILTER_TEST_UTILS_FAKE_TAB_INTERFACE_H_
