// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.test_dummy;

import org.chromium.chrome.features.test_dummy.TestDummy;
import org.chromium.components.module_installer.builder.ModuleInterface;

/** Provides the test dummy implementation. */
@ModuleInterface(module = "test_dummy",
        impl = "org.chromium.chrome.modules.test_dummy.TestDummyProviderImpl")
public interface TestDummyProvider {
    TestDummy getTestDummy();
}
