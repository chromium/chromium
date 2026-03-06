// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/test_utils/fake_tab_interface.h"

#include "chrome/browser/profiles/profile.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace multistep_filter {

FakeTabInterface::FakeTabInterface(Profile* profile,
                                   content::WebContents* web_contents)
    : web_contents_(web_contents) {
  if (profile) {
    ON_CALL(mock_browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile));
  }
}

FakeTabInterface::~FakeTabInterface() = default;

content::WebContents* FakeTabInterface::GetContents() const {
  return web_contents_;
}

BrowserWindowInterface* FakeTabInterface::GetBrowserWindowInterface() {
  return &mock_browser_window_interface_;
}

const BrowserWindowInterface* FakeTabInterface::GetBrowserWindowInterface()
    const {
  return &mock_browser_window_interface_;
}

}  // namespace multistep_filter
