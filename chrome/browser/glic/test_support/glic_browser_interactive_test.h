// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_INTERACTIVE_TEST_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_INTERACTIVE_TEST_H_

#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"

namespace glic {
class GlicBrowserInteractiveTest
    : public GlicBrowserTestMixin<InteractiveBrowserTest> {};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_INTERACTIVE_TEST_H_
