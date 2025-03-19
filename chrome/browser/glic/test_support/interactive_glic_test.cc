// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/interactive_glic_test.h"

#include "chrome/test/interaction/interactive_browser_test.h"

namespace glic::test {

const InteractiveBrowserTestApi::DeepQuery kPathToMockGlicCloseButton = {
    "#closebn"};
const InteractiveBrowserTestApi::DeepQuery kPathToGuestPanel = {
    ".panel#guestPanel"};

}  // namespace glic::test
