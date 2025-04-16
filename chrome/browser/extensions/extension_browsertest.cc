// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

namespace extensions {

ExtensionBrowserTest::ExtensionBrowserTest(ContextType context_type)
    : ExtensionPlatformBrowserTest(context_type) {}

ExtensionBrowserTest::~ExtensionBrowserTest() = default;

}  // namespace extensions
