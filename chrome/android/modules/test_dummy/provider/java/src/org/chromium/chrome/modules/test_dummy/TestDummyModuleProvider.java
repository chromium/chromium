// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.test_dummy;

import org.chromium.components.module_installer.engine.InstallListener;

/** Installs and loads the test dummy module. */
public class TestDummyModuleProvider {
    /** Returns true if the module is installed. */
    public static boolean isModuleInstalled() {
        return TestDummyModule.isInstalled();
    }

    /**
     * Installs the module.
     *
     * Can only be called if the module is not installed.
     *
     * @param listener Called when the install has finished.
     */
    public static void installModule(InstallListener listener) {
        TestDummyModule.install(listener);
    }

    /**
     * Returns the test dummy provider from inside the module.
     *
     * Can only be called if the module is installed. Maps native resources into memory on first
     * call.
     */
    public static TestDummyProvider getTestDummyProvider() {
        return TestDummyModule.getImpl();
    }
}
