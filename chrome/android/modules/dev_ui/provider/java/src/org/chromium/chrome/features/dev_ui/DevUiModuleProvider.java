// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.dev_ui;

import org.chromium.base.annotations.CalledByNative;

/** Helpers for DevUI DFM installation. */
public class DevUiModuleProvider {
    @CalledByNative
    private static boolean isModuleInstalled() {
        return DevUiModule.isInstalled();
    }

    @CalledByNative
    private static void installModule(DevUiInstallListener listener) {
        DevUiModule.install(listener);
    }

    @CalledByNative
    private static void loadModule() {
        // Native resource are loaded as side effect of first getImpl() call.
        DevUiModule.getImpl();
    }
}
