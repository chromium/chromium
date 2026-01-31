// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_FAKE_DEFAULT_BROWSER_SETTER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_FAKE_DEFAULT_BROWSER_SETTER_H_

#include "chrome/browser/default_browser/default_browser_setter.h"

namespace default_browser {

class FakeDefaultBrowserSetter : public DefaultBrowserSetter {
 public:
  FakeDefaultBrowserSetter() = default;

  DefaultBrowserSetterType GetType() const override;
  void Execute(DefaultBrowserSetterCompletionCallback on_complete) override;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_FAKE_DEFAULT_BROWSER_SETTER_H_
